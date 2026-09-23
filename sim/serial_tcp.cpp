#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include "Arduino.h"

#define SIM_PORT 8766

SimSerial Serial;

static SOCKET listen_sock = INVALID_SOCKET;
static SOCKET client_sock = INVALID_SOCKET;

// Accepts the bridge connection once it arrives (non-blocking)
static void try_accept()
{
    if (client_sock != INVALID_SOCKET || listen_sock == INVALID_SOCKET) return;
    SOCKET c = accept(listen_sock, nullptr, nullptr);
    if (c != INVALID_SOCKET)
    {
        u_long nb = 1;
        ioctlsocket(c, FIONBIO, &nb);
        client_sock = c;
        printf("[sim] bridge connected\n");
        fflush(stdout);
    }
}

static void drop_client()
{
    if (client_sock != INVALID_SOCKET) closesocket(client_sock);
    client_sock = INVALID_SOCKET;
    printf("[sim] bridge disconnected\n");
    fflush(stdout);
}

// Opens the TCP server the bridge connects to (instead of a serial port)
void SimSerial::begin(unsigned long)
{
    WSADATA w;
    WSAStartup(MAKEWORD(2, 2), &w);
    listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));
    sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(SIM_PORT);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bind(listen_sock, (sockaddr *)&a, sizeof(a));
    listen(listen_sock, 1);
    u_long nb = 1;
    ioctlsocket(listen_sock, FIONBIO, &nb);
    printf("[sim] listening for bridge on 127.0.0.1:%d\n", SIM_PORT);
    fflush(stdout);
}

int SimSerial::available()
{
    try_accept();
    if (client_sock == INVALID_SOCKET) return 0;
    u_long n = 0;
    ioctlsocket(client_sock, FIONREAD, &n);
    return (int)n;
}

int SimSerial::read()
{
    if (client_sock == INVALID_SOCKET) return -1;
    char c;
    int r = recv(client_sock, &c, 1, 0);
    if (r == 1) return (unsigned char)c;
    if (r == 0) drop_client();
    return -1;
}

size_t SimSerial::readBytes(uint8_t *buf, size_t n)
{
    size_t got = 0;
    uint32_t start = millis();
    while (got < n)
    {
        if (client_sock == INVALID_SOCKET) break;
        int r = recv(client_sock, (char *)buf + got, (int)(n - got), 0);
        if (r > 0)
        {
            got += r;
            start = millis();
        }
        else if (r == 0)
        {
            drop_client();
            break;
        }
        else
        {
            if (millis() - start > timeout_) break;
            Sleep(1);
        }
    }
    return got;
}

void SimSerial::print(const char *s)
{
    try_accept();
    if (client_sock == INVALID_SOCKET) return;
    if (send(client_sock, s, (int)strlen(s), 0) == SOCKET_ERROR) drop_client();
}

void SimSerial::println(const char *s)
{
    print(s);
    print("\r\n");
}
