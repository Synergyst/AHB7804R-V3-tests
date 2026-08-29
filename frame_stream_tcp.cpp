#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/types.h>
#include <algorithm>

static uint64_t now_ms(){
    timeval tv{};
    gettimeofday(&tv, nullptr);
    return (uint64_t)tv.tv_sec*1000ULL + (uint64_t)tv.tv_usec/1000ULL;
}

static bool is_space(char c){ return c==' '||c=='\t'||c=='\r'||c=='\n'; }
static std::vector<std::string> split_ws(const std::string& s){
    std::vector<std::string> out;
    size_t i=0, n=s.size();
    while(i<n){
        while(i<n && is_space(s[i])) i++;
        if(i>=n) break;
        size_t j=i;
        while(j<n && !is_space(s[j])) j++;
        out.emplace_back(s.substr(i,j-i));
        i=j;
    }
    return out;
}
static std::string slurp(const char* path){
    int fd=open(path,O_RDONLY);
    if(fd<0) return {};
    std::string s;
    char buf[4096];
    for(;;){
        ssize_t r=read(fd,buf,sizeof(buf));
        if(r<=0) break;
        s.append(buf, buf+r);
    }
    close(fd);
    return s;
}
static bool parse_u32(const std::string& t, uint32_t& v){
    char* end=nullptr;
    unsigned long x=strtoul(t.c_str(), &end, 0);
    if(end==t.c_str() || *end!='\0') return false;
    v=(uint32_t)x;
    return true;
}
static bool parse_u64(const std::string& t, uint64_t& v){
    char* end=nullptr;
    unsigned long long x=strtoull(t.c_str(), &end, 0);
    if(end==t.c_str() || *end!='\0') return false;
    v=(uint64_t)x;
    return true;
}

struct Pool { int id=-1; uint32_t phys=0, blksz=0, blkcnt=0; };

static bool vb_ready_and_find_pool(int want_pool, Pool& out_pool, int& max_pools_out){
    max_pools_out = 0;
    out_pool = Pool{};

    std::string txt = slurp("/proc/umap/vb");
    if(txt.empty()) return false;

    size_t mp = txt.find("Max Count of Pools:");
    if(mp != std::string::npos){
        size_t eol = txt.find('\n', mp);
        std::string line = txt.substr(mp, (eol==std::string::npos)?std::string::npos:(eol-mp));
        auto t = split_ws(line);
        if(!t.empty()){
            uint32_t v=0;
            if(parse_u32(t.back(), v)) max_pools_out = (int)v;
        }
    }
    if(max_pools_out <= 0) return false;

    size_t pos=0;
    while(pos < txt.size()){
        size_t e = txt.find('\n', pos);
        if(e==std::string::npos) e=txt.size();
        std::string line = txt.substr(pos, e-pos);
        pos = e+1;

        auto t = split_ws(line);
        if(t.size() < 6) continue;

        char* end=nullptr;
        long pid = strtol(t[0].c_str(), &end, 10);
        if(end==t[0].c_str() || *end!='\0') continue;
        if((int)pid != want_pool) continue;

        Pool p{};
        p.id=(int)pid;
        if(!parse_u32(t[1], p.phys)) continue;
        if(!parse_u32(t[4], p.blksz)) continue;
        if(!parse_u32(t[5], p.blkcnt)) continue;

        if(p.phys==0 || p.blksz==0 || p.blkcnt==0) continue;

        out_pool = p;
        return true;
    }
    return false;
}

/* ---------------- Sofia management ---------------- */

static bool str_contains(const std::string& hay, const std::string& needle){
    return hay.find(needle) != std::string::npos;
}

static std::string read_cmdline(pid_t pid){
    char path[128];
    std::snprintf(path, sizeof(path), "/proc/%d/cmdline", (int)pid);
    std::string s = slurp(path);
    // /proc/pid/cmdline is NUL-separated; keep as-is but also make a printable version for searching
    for(char& c : s) if(c=='\0') c=' ';
    return s;
}

static std::vector<pid_t> find_pids_by_cmd_substr(const std::string& needle){
    std::vector<pid_t> pids;
    DIR* d = opendir("/proc");
    if(!d) return pids;

    dirent* de=nullptr;
    while((de=readdir(d))!=nullptr){
        if(de->d_name[0] < '0' || de->d_name[0] > '9') continue;
        pid_t pid = (pid_t)std::atoi(de->d_name);
        if(pid <= 1) continue;

        std::string cmd = read_cmdline(pid);
        if(cmd.empty()) continue;

        if(str_contains(cmd, needle)){
            pids.push_back(pid);
        }
    }
    closedir(d);
    return pids;
}

static bool pid_alive(pid_t pid){
    return (pid > 1) && (kill(pid, 0) == 0);
}

static void kill_pids(const std::vector<pid_t>& pids, bool verbose){
    for(pid_t pid : pids){
        if(pid_alive(pid)){
            if(verbose) std::fprintf(stderr, "[sofia] SIGTERM pid=%d\n", (int)pid);
            kill(pid, SIGTERM);
        }
    }
    // brief wait
    usleep(300*1000);

    for(pid_t pid : pids){
        if(pid_alive(pid)){
            if(verbose) std::fprintf(stderr, "[sofia] SIGKILL pid=%d\n", (int)pid);
            kill(pid, SIGKILL);
        }
    }
}

static pid_t spawn_detached(const char* path){
    pid_t pid = fork();
    if(pid < 0) return -1;
    if(pid == 0){
        setsid();
        int dn = open("/dev/null", O_RDWR);
        if(dn >= 0){
            dup2(dn, 0);
            dup2(dn, 1);
            dup2(dn, 2);
            if(dn > 2) close(dn);
        }
        char* const argv[] = { (char*)path, nullptr };
        execv(path, argv);
        _exit(127);
    }
    return pid;
}

static void ensure_sofia(const char* sofia_path, bool start_sofia, bool restart_sofia, bool verbose){
    // We treat "running" as any process whose cmdline contains sofia_path or ends with "/Sofia"
    std::string needle1 = sofia_path;
    std::string needle2 = "/Sofia";

    auto pids = find_pids_by_cmd_substr(needle2);
    // Also try path match (covers renamed binaries / symlinks)
    auto pids2 = find_pids_by_cmd_substr(needle1);
    pids.insert(pids.end(), pids2.begin(), pids2.end());

    // de-dup
    std::sort(pids.begin(), pids.end());
    pids.erase(std::unique(pids.begin(), pids.end()), pids.end());

    bool running = false;
    for(pid_t pid: pids) if(pid_alive(pid)) { running=true; break; }

    if(restart_sofia){
        if(verbose) std::fprintf(stderr, "[sofia] restart requested; running=%s\n", running?"YES":"NO");
        if(running) kill_pids(pids, verbose);
        pid_t np = spawn_detached(sofia_path);
        if(verbose) std::fprintf(stderr, "[sofia] started pid=%d path=%s\n", (int)np, sofia_path);
        return;
    }

    if(start_sofia){
        if(running){
            if(verbose) std::fprintf(stderr, "[sofia] already running; not restarting\n");
            return;
        }
        pid_t np = spawn_detached(sofia_path);
        if(verbose) std::fprintf(stderr, "[sofia] started pid=%d path=%s\n", (int)np, sofia_path);
    }
}

/* ---------------- Networking + copy ---------------- */

static int connect_blocking(const char* ip, int port){
    int s=socket(AF_INET, SOCK_STREAM, 0);
    if(s<0) return -1;
    sockaddr_in addr{};
    addr.sin_family=AF_INET;
    addr.sin_port=htons((uint16_t)port);
    if(inet_pton(AF_INET, ip, &addr.sin_addr)!=1){
        close(s); return -1;
    }
    if(connect(s, (sockaddr*)&addr, sizeof(addr))!=0){
        close(s); return -1;
    }
    return s;
}

static bool send_all_blocking(int fd, const void* p, size_t n){
    const uint8_t* b=(const uint8_t*)p;
    size_t off=0;
    while(off<n){
        ssize_t w = send(fd, b+off, n-off, 0);
        if(w<=0) return false;
        off += (size_t)w;
    }
    return true;
}

struct MapState {
    uint64_t phys=0;
    uint8_t* map=nullptr;
    size_t maplen=0;
    uint32_t off=0;
};
static void unmap_state2(MapState& st){
    if(st.map){
        munmap(st.map, st.maplen);
        st.map=nullptr; st.maplen=0; st.off=0; st.phys=0;
    }
}
static bool map_block2(int memfd, uint64_t phys, size_t blksz, MapState& st){
    if(st.map && st.phys==phys) return true;
    unmap_state2(st);

    long pagesz = sysconf(_SC_PAGESIZE);
    uint64_t base = phys & ~((uint64_t)pagesz - 1);
    st.off = (uint32_t)(phys - base);
    st.maplen = st.off + blksz;
    void* p = mmap(nullptr, st.maplen, PROT_READ, MAP_SHARED, memfd, (off_t)base);
    if(p==MAP_FAILED){
        st.map=nullptr; st.maplen=0; st.off=0; st.phys=0;
        return false;
    }
    st.map=(uint8_t*)p;
    st.phys=phys;
    return true;
}

#pragma pack(push,1)
struct FrameHdr {
    uint32_t magic;     // 'YFRM'
    uint32_t seq;
    uint32_t w;
    uint32_t h;
    uint32_t bytes;
    uint64_t phys;
    uint32_t x;
    uint32_t y;
    uint32_t down;
};
#pragma pack(pop)

enum Region { FULL, TL, TR, BL, BR };
enum ScanMode { SCAN_ONCE, SCAN_PERIODIC, SCAN_OFF };

static void copy_crop_down_y(uint8_t* dst, uint32_t out_w, uint32_t out_h,
                            const uint8_t* Y, uint32_t stride,
                            uint32_t x0, uint32_t y0,
                            uint32_t down){
    if(down==1){
        for(uint32_t r=0;r<out_h;r++){
            const uint8_t* src = Y + (size_t)(y0 + r)*stride + x0;
            std::memcpy(dst + (size_t)r*out_w, src, out_w);
        }
    } else {
        for(uint32_t r=0;r<out_h;r++){
            const uint8_t* src = Y + (size_t)(y0 + r*2)*stride + x0;
            uint8_t* drow = dst + (size_t)r*out_w;
            for(uint32_t c=0;c<out_w;c++) drow[c] = src[c*2];
        }
    }
}

static void usage(const char* a){
    std::fprintf(stderr,
      "usage: %s --ip A.B.C.D --port P (--full|--tl|--tr|--bl|--br)\n"
      "          [--pool 5] [--down 1|2] [--fps N]\n"
      "          [--scan once|periodic|off] [--rescan-ms MS] [--phys 0xADDR]\n"
      "          [--wait-vb-ms MS] [--wait-step-ms MS] [--no-wait-vb]\n"
      "          [--stats-ms MS] [--no-stats] [--max-frames N]\n"
      "          [--reconnect|--no-reconnect]\n"
      "          [--start-sofia] [--restart-sofia] [--sofia-path PATH]\n",
      a);
}

int main(int argc, char** argv){
    const char* ip=nullptr;
    int port=0;

    int pool_id=5;
    uint32_t down=2;
    int fps=0;

    ScanMode scan_mode=SCAN_ONCE;
    uint64_t rescan_ms=30000;
    uint64_t fixed_phys=0;

    bool wait_vb=true;
    uint64_t wait_vb_ms=120000;
    uint64_t wait_step_ms=500;

    uint64_t stats_ms=1000;
    bool no_stats=false;
    uint64_t max_frames=0;
    bool reconnect=true;

    bool start_sofia=false;
    bool restart_sofia=false;

    // Please plop your Sofia binary somewhere where the device can access it!
    const char* sofia_path="/home/AHB7804R-V3-FW-dump-with-mods/Sofia";

    Region region=FULL;
    bool region_set=false;

    for(int i=1;i<argc;i++){
        if(!std::strcmp(argv[i],"--ip") && i+1<argc) ip=argv[++i];
        else if(!std::strcmp(argv[i],"--port") && i+1<argc) port=std::atoi(argv[++i]);

        else if(!std::strcmp(argv[i],"--pool") && i+1<argc) pool_id=std::atoi(argv[++i]);
        else if(!std::strcmp(argv[i],"--down") && i+1<argc) down=(uint32_t)std::strtoul(argv[++i],nullptr,0);
        else if(!std::strcmp(argv[i],"--fps") && i+1<argc) fps=std::atoi(argv[++i]);

        else if(!std::strcmp(argv[i],"--scan") && i+1<argc){
            const char* m=argv[++i];
            if(!std::strcmp(m,"once")) scan_mode=SCAN_ONCE;
            else if(!std::strcmp(m,"periodic")) scan_mode=SCAN_PERIODIC;
            else if(!std::strcmp(m,"off")) scan_mode=SCAN_OFF;
            else { usage(argv[0]); return 2; }
        }
        else if(!std::strcmp(argv[i],"--rescan-ms") && i+1<argc) rescan_ms=(uint64_t)std::strtoull(argv[++i],nullptr,0);
        else if(!std::strcmp(argv[i],"--phys") && i+1<argc){ parse_u64(argv[++i], fixed_phys); }

        else if(!std::strcmp(argv[i],"--wait-vb-ms") && i+1<argc) wait_vb_ms=(uint64_t)std::strtoull(argv[++i],nullptr,0);
        else if(!std::strcmp(argv[i],"--wait-step-ms") && i+1<argc) wait_step_ms=(uint64_t)std::strtoull(argv[++i],nullptr,0);
        else if(!std::strcmp(argv[i],"--no-wait-vb")) wait_vb=false;

        else if(!std::strcmp(argv[i],"--stats-ms") && i+1<argc) stats_ms=(uint64_t)std::strtoull(argv[++i],nullptr,0);
        else if(!std::strcmp(argv[i],"--no-stats")) no_stats=true;
        else if(!std::strcmp(argv[i],"--max-frames") && i+1<argc) max_frames=(uint64_t)std::strtoull(argv[++i],nullptr,0);

        else if(!std::strcmp(argv[i],"--reconnect")) reconnect=true;
        else if(!std::strcmp(argv[i],"--no-reconnect")) reconnect=false;

        else if(!std::strcmp(argv[i],"--start-sofia")) start_sofia=true;
        else if(!std::strcmp(argv[i],"--restart-sofia")) restart_sofia=true;
        else if(!std::strcmp(argv[i],"--sofia-path") && i+1<argc) sofia_path=argv[++i];

        else if(!std::strcmp(argv[i],"--full")){ region=FULL; region_set=true; }
        else if(!std::strcmp(argv[i],"--tl")){ region=TL; region_set=true; }
        else if(!std::strcmp(argv[i],"--tr")){ region=TR; region_set=true; }
        else if(!std::strcmp(argv[i],"--bl")){ region=BL; region_set=true; }
        else if(!std::strcmp(argv[i],"--br")){ region=BR; region_set=true; }
        else { usage(argv[0]); return 2; }
    }

    if(!ip || port<=0 || !region_set){ usage(argv[0]); return 2; }
    if(down!=1 && down!=2){ std::fprintf(stderr,"--down must be 1 or 2\n"); return 2; }
    if(restart_sofia) start_sofia=true; // implied
    if(fixed_phys) scan_mode=SCAN_OFF;
    if(scan_mode==SCAN_OFF && fixed_phys==0){
        std::fprintf(stderr, "--scan off requires --phys 0xADDR\n");
        return 2;
    }

    // Sofia control (if requested)
    ensure_sofia(sofia_path, start_sofia, restart_sofia, !no_stats);

    int memfd=open("/dev/mem", O_RDONLY|O_SYNC);
    if(memfd<0){ perror("open(/dev/mem)"); return 1; }

    // Wait for VB pool unless disabled
    Pool pool{};
    int maxp=0;
    if(wait_vb && scan_mode!=SCAN_OFF){
        uint64_t start = now_ms();
        while(true){
            if(vb_ready_and_find_pool(pool_id, pool, maxp)) break;
            if(now_ms() - start >= wait_vb_ms){
                std::fprintf(stderr, "timeout waiting for vb pool %d (MaxPools=%d)\n", pool_id, maxp);
                return 1;
            }
            if(!no_stats && (now_ms() - start) < 2000){
                std::fprintf(stderr, "[vb] waiting for pools (Sofia not started?)\n");
            }
            usleep((useconds_t)wait_step_ms * 1000);
        }
        if(!no_stats){
            std::fprintf(stderr, "[vb] pool %d phys=0x%08x blksz=0x%x blkcnt=%u\n", pool.id, pool.phys, pool.blksz, pool.blkcnt);
        }
    } else if(scan_mode!=SCAN_OFF){
        // no wait: try once
        if(!vb_ready_and_find_pool(pool_id, pool, maxp)){
            std::fprintf(stderr, "pool %d not found\n", pool_id);
            return 1;
        }
    }

    // Geometry (pool5 / full frame assumption)
    const uint32_t STRIDE=1920;
    uint32_t crop_w=1920, crop_h=1080, x0=0, y0=0;
    if(region!=FULL){
        crop_w=960; crop_h=540;
        if(region==TL){ x0=0;   y0=0;   }
        if(region==TR){ x0=960; y0=0;   }
        if(region==BL){ x0=0;   y0=540; }
        if(region==BR){ x0=960; y0=540; }
    }
    uint32_t out_w = (down==1) ? crop_w : (crop_w/2);
    uint32_t out_h = (down==1) ? crop_h : (crop_h/2);
    size_t out_bytes = (size_t)out_w*out_h;
    std::vector<uint8_t> outbuf(out_bytes);

    // Choose phys
    uint64_t current_phys = 0;
    uint32_t blksz = 0;

    if(scan_mode==SCAN_OFF){
        current_phys = fixed_phys;
        blksz = 0x2f7600; // safe default for 1920*1080*3/2; mapping needs length
    } else {
        current_phys = (uint64_t)pool.phys;
        blksz = pool.blksz;
    }

    MapState ms2;
    if(!map_block2(memfd, current_phys, blksz, ms2)){
        std::fprintf(stderr, "mmap failed for phys=0x%llx\n", (unsigned long long)current_phys);
        return 1;
    }
    if(!no_stats) std::fprintf(stderr, "[map] phys=0x%llx\n", (unsigned long long)current_phys);

    int s=-1;
    while(true){
        s = connect_blocking(ip, port);
        if(s>=0) break;
        if(!reconnect){
            std::fprintf(stderr, "connect failed\n");
            return 1;
        }
        usleep(500000);
    }
    if(!no_stats) std::fprintf(stderr, "[net] connected\n");

    uint64_t next_rescan = now_ms() + rescan_ms;
    uint64_t last_stats = now_ms();
    uint64_t frames=0, bytes=0;
    uint32_t seq=0;

    useconds_t sleep_us = (fps>0) ? (useconds_t)(1000000 / fps) : 0;

    for(;;){
        uint64_t t = now_ms();

        if(scan_mode==SCAN_PERIODIC && scan_mode!=SCAN_OFF && t >= next_rescan){
            Pool p2{}; int mp2=0;
            if(vb_ready_and_find_pool(pool_id, p2, mp2)){
                // simplest: just use first block in pool
                uint64_t new_phys = (uint64_t)p2.phys;
                if(new_phys && new_phys != current_phys){
                    current_phys = new_phys;
                    blksz = p2.blksz;
                    map_block2(memfd, current_phys, blksz, ms2);
                    if(!no_stats) std::fprintf(stderr, "[rescan] phys=0x%llx\n", (unsigned long long)current_phys);
                }
            }
            next_rescan = t + rescan_ms;
        }

        const uint8_t* Y = (const uint8_t*)(ms2.map + ms2.off);
        copy_crop_down_y(outbuf.data(), out_w, out_h, Y, STRIDE, x0, y0, down);

        FrameHdr hdr{};
        hdr.magic=0x5946524d;
        hdr.seq=seq++;
        hdr.w=out_w; hdr.h=out_h;
        hdr.bytes=(uint32_t)out_bytes;
        hdr.phys=current_phys;
        hdr.x=x0; hdr.y=y0;
        hdr.down=down;

        bool ok = send_all_blocking(s, &hdr, sizeof(hdr)) && send_all_blocking(s, outbuf.data(), outbuf.size());
        if(!ok){
            close(s); s=-1;
            if(!reconnect) break;
            while(true){
                s = connect_blocking(ip, port);
                if(s>=0) break;
                usleep(500000);
            }
            if(!no_stats) std::fprintf(stderr, "[net] reconnected\n");
            continue;
        }

        frames++;
        bytes += sizeof(hdr) + outbuf.size();

        if(!no_stats && (t - last_stats >= stats_ms)){
            double dt = (double)(t - last_stats)/1000.0;
            double fps_now = (dt>0)? ((double)frames/dt) : 0.0;
            double mbps = (dt>0)? ((double)bytes*8.0/dt/1000000.0) : 0.0;
            std::fprintf(stderr, "[stats] %.2f fps %.2f Mbps out=%ux%u down=%u\n", fps_now, mbps, out_w, out_h, down);
            frames=0; bytes=0;
            last_stats = t;
        }

        if(max_frames && hdr.seq >= max_frames) break;
        if(sleep_us) usleep(sleep_us);
        else usleep(1000);
    }

    unmap_state2(ms2);
    close(s);
    close(memfd);
    return 0;
}
