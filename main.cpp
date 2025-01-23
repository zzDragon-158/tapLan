#include "tapLanServer.hpp"
#include "tapLanClient.hpp"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

bool isServer = false;
bool isClient = false;
char serverAddr[64];
uint16_t serverPort = 0;
uint16_t clientPort = 0;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("less argc!\n");
        return 1;
    }
    int opt;
    while ((opt = getopt(argc, argv, "s:c:a:p:")) != -1) {
        switch (opt) {
            case 's':
                isServer = true;
                serverPort = atoi(optarg);
                printf("serverPort: %u\n", serverPort);
                break;
            case 'c':
                isClient = true;
                clientPort = atoi(optarg);
                printf("clientPort: %u\n", clientPort);
                break;
            case 'a':
                strcpy(serverAddr, optarg);
                printf("serverAddr: %s\n", serverAddr);
                break;
            case 'p':
                serverPort = atoi(optarg);
                printf("serverPort: %u\n", serverPort);
                break;
            case '?':
                // 无效选项或缺少参数
                fprintf(stderr, "Usage: %s [-h] [-o <filename>]\n", argv[0]);
                return 1;
        }
    }
    if (isServer) {
        TapLanServer tLS(serverPort);
        tLS.start();
        while (1);
    } else if (isClient) {
        TapLanClient tLC(serverAddr, serverPort, clientPort);
        tLC.start();
        while (1);
    }
    return 0;
}
