#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>

static void usage(const char* argv0){
    std::fprintf(stderr,
        "usage: %s [options]\n"
        "\n"
        "Supervises Sofia by alternating SIGCONT/SIGSTOP to avoid WDT trips,\n"
        "and runs frame_stream_tcp with safe defaults (unless overridden).\n"
        "\n"
        "Sofia toggle options:\n"
        "  --sofia-name Sofia         Process name substring to match in /proc/*/cmdline\n"
        "  --alive-ms 1000            How long Sofia is allowed to run (SIGCONT period)\n"
        "  --dead-ms  9000            How long Sofia is stopped (SIGSTOP period)\n"
        "  --no-stop                  Never SIGSTOP (only SIGCONT periodically)\n"
        "  --no-continue              Never SIGCONT (only SIGSTOP periodically)\n"
        "\n"
        "frame_stream_tcp options (defaults match your requested command):\n"
        "  --frame-stream PATH        Path to frame_stream_tcp\n"
        "  --ip A.B.C.D               Default: 192.168.168.37\n"
        "  --port N                   Default: 5000\n"
        "  --pool N                   Default: 5\n"
        "  --tl/--tr/--bl/--br/--full  Default: --tl\n"
        "  --down N                   Default: 2\n"
        "  --fps N                    Default: 0\n"
        "  --wait-vb-ms N             Default: 120000\n"
        "  --wait-step-ms N           Default: 500\n"
        "  --no-stats                 Default: enabled\n"
        "  --reconnect                Default: enabled\n"
        "  --restart-sofia            Default: enabled\n"
        "\n",
        argv0);
}

static std::string slurp(const char* path){
    int fd = open(path, O_RDONLY);
    if(fd < 0) return {};
    std::string s;
    char buf[4096];
    for(;;){
        ssize_t r = read(fd, buf, sizeof(buf));
        if(r <= 0) break;
        s.append(buf, buf + r);
    }
    close(fd);
    return s;
}

static bool is_digits(const char* s){
    if(!s || !*s) return false;
    for(const char* p=s; *p; p++){
        if(*p < '0' || *p > '9') return false;
    }
    return true;
}

static std::vector<pid_t> find_pids_by_cmdline_substr(const std::string& needle){
    std::vector<pid_t> out;
    DIR* d = opendir("/proc");
    if(!d) return out;

    dirent* de=nullptr;
    while((de=readdir(d))!=nullptr){
        if(!is_digits(de->d_name)) continue;
        pid_t pid = (pid_t)std::atoi(de->d_name);
        if(pid <= 1) continue;

        char path[128];
        std::snprintf(path, sizeof(path), "/proc/%d/cmdline", (int)pid);

        std::string cmd = slurp(path);
        if(cmd.empty()) continue;

        // cmdline is NUL-separated; convert NUL -> space
        for(char& c : cmd) if(c=='\0') c=' ';

        if(cmd.find(needle) != std::string::npos){
            out.push_back(pid);
        }
    }
    closedir(d);
    return out;
}

static void signal_pids(const std::vector<pid_t>& pids, int sig){
    for(pid_t pid : pids){
        if(pid > 1) kill(pid, sig);
    }
}

static void msleep(uint64_t ms){
    usleep((useconds_t)(ms * 1000));
}

static pid_t spawn_frame_stream(const std::vector<std::string>& args){
    pid_t pid = fork();
    if(pid < 0) return -1;
    if(pid == 0){
        std::vector<char*> argv;
        argv.reserve(args.size()+1);
        for(const auto& s: args) argv.push_back((char*)s.c_str());
        argv.push_back(nullptr);

        execv(argv[0], argv.data());
        _exit(127);
    }
    return pid;
}

int main(int argc, char** argv){
    // Sofia defaults
    std::string sofia_name = "Sofia";
    uint64_t alive_ms = 1000;
    uint64_t dead_ms  = 9000;
    bool do_stop = true;
    bool do_cont = true;

    // frame_stream_tcp defaults (your requested safe defaults)
    std::string frame_stream_path = "/home/AHB7804R-V3-FW-dump-with-mods/frame-manip/frame_stream_tcp";
    std::string ip = "192.168.168.37";
    int port = 5000;
    int pool = 5;
    std::string region = "--tl"; // default
    int down = 2;
    int fps = 0;
    uint64_t wait_vb_ms = 120000;
    uint64_t wait_step_ms = 500;

    bool no_stats = true;
    bool reconnect = true;
    bool restart_sofia = true;

    for(int i=1;i<argc;i++){
        if(!std::strcmp(argv[i],"--sofia-name") && i+1<argc) sofia_name = argv[++i];
        else if(!std::strcmp(argv[i],"--alive-ms") && i+1<argc) alive_ms = (uint64_t)std::strtoull(argv[++i], nullptr, 0);
        else if(!std::strcmp(argv[i],"--dead-ms") && i+1<argc) dead_ms = (uint64_t)std::strtoull(argv[++i], nullptr, 0);
        else if(!std::strcmp(argv[i],"--no-stop")) do_stop = false;
        else if(!std::strcmp(argv[i],"--no-continue")) do_cont = false;

        else if(!std::strcmp(argv[i],"--frame-stream") && i+1<argc) frame_stream_path = argv[++i];
        else if(!std::strcmp(argv[i],"--ip") && i+1<argc) ip = argv[++i];
        else if(!std::strcmp(argv[i],"--port") && i+1<argc) port = std::atoi(argv[++i]);
        else if(!std::strcmp(argv[i],"--pool") && i+1<argc) pool = std::atoi(argv[++i]);
        else if(!std::strcmp(argv[i],"--full")) region="--full";
        else if(!std::strcmp(argv[i],"--tl")) region="--tl";
        else if(!std::strcmp(argv[i],"--tr")) region="--tr";
        else if(!std::strcmp(argv[i],"--bl")) region="--bl";
        else if(!std::strcmp(argv[i],"--br")) region="--br";
        else if(!std::strcmp(argv[i],"--down") && i+1<argc) down = std::atoi(argv[++i]);
        else if(!std::strcmp(argv[i],"--fps") && i+1<argc) fps = std::atoi(argv[++i]);
        else if(!std::strcmp(argv[i],"--wait-vb-ms") && i+1<argc) wait_vb_ms = (uint64_t)std::strtoull(argv[++i], nullptr, 0);
        else if(!std::strcmp(argv[i],"--wait-step-ms") && i+1<argc) wait_step_ms = (uint64_t)std::strtoull(argv[++i], nullptr, 0);

        else if(!std::strcmp(argv[i],"--no-stats")) no_stats = true;
        else if(!std::strcmp(argv[i],"--stats")) no_stats = false;

        else if(!std::strcmp(argv[i],"--reconnect")) reconnect = true;
        else if(!std::strcmp(argv[i],"--no-reconnect")) reconnect = false;

        else if(!std::strcmp(argv[i],"--restart-sofia")) restart_sofia = true;
        else if(!std::strcmp(argv[i],"--no-restart-sofia")) restart_sofia = false;

        else if(!std::strcmp(argv[i],"--help") || !std::strcmp(argv[i],"-h")){
            usage(argv[0]);
            return 0;
        } else {
            std::fprintf(stderr, "unknown arg: %s\n", argv[i]);
            usage(argv[0]);
            return 2;
        }
    }

    // Build argv for frame_stream_tcp (no shell)
    std::vector<std::string> fs;
    fs.push_back(frame_stream_path);
    if(restart_sofia) fs.push_back("--restart-sofia");
    if(reconnect) fs.push_back("--reconnect");
    fs.push_back("--ip"); fs.push_back(ip);
    fs.push_back("--port"); fs.push_back(std::to_string(port));
    fs.push_back("--pool"); fs.push_back(std::to_string(pool));
    fs.push_back(region);
    fs.push_back("--down"); fs.push_back(std::to_string(down));
    fs.push_back("--fps"); fs.push_back(std::to_string(fps));
    if(no_stats) fs.push_back("--no-stats");
    fs.push_back("--wait-vb-ms"); fs.push_back(std::to_string((unsigned long long)wait_vb_ms));
    fs.push_back("--wait-step-ms"); fs.push_back(std::to_string((unsigned long long)wait_step_ms));

    // Start frame_stream_tcp child
    pid_t child = spawn_frame_stream(fs);
    if(child < 0){
        std::perror("fork");
        return 1;
    }
    std::fprintf(stderr, "[supervisor] started frame_stream_tcp pid=%d\n", (int)child);

    // Sofia toggle loop
    for(;;){
        // If frame_stream_tcp exits, we exit too.
        int status=0;
        pid_t r = waitpid(child, &status, WNOHANG);
        if(r == child){
            std::fprintf(stderr, "[supervisor] frame_stream_tcp exited status=0x%x\n", status);
            return 0;
        }

        auto pids = find_pids_by_cmdline_substr(sofia_name);
        if(!pids.empty()){
            if(do_cont){
                signal_pids(pids, SIGCONT);
                // optional: could log
            }
        }
        if(alive_ms) msleep(alive_ms);

        // check again whether child exited
        r = waitpid(child, &status, WNOHANG);
        if(r == child){
            std::fprintf(stderr, "[supervisor] frame_stream_tcp exited status=0x%x\n", status);
            return 0;
        }

        pids = find_pids_by_cmdline_substr(sofia_name);
        if(!pids.empty()){
            if(do_stop){
                signal_pids(pids, SIGSTOP);
            }
        }
        if(dead_ms) msleep(dead_ms);
    }

    return 0;
}
