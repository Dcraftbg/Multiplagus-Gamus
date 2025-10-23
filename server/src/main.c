#include <snet.h>
#include <assert.h>
#include <gt.h>
#include <stdlib.h>
#include "eprintf.h"

#define PORT 8080
typedef struct {
    int fd;
} Client;
void client_thread(void* client_void) {
    Client* client = client_void;
    for(;;) {
        char buf[128];
        gtblockfd(client->fd, GTBLOCKIN);
        int n = recv(client->fd, buf, sizeof(buf), 0);
        if(n < 0) eprintfln("ERROR:%d: when receiving: %s", client->fd, sneterr());
        if(n <= 0) break;
        eprintfln("INFO:%d: %.*s", client->fd, n, buf);
        gtblockfd(client->fd, GTBLOCKOUT);
        send(client->fd, buf, n, 0);
    }
    eprintfln("INFO:%d: Closing", client->fd);
    closesocket(client->fd);
    free(client);
}
void printer_thread(void* arg0) {
    unsigned long secs = (unsigned long) arg0;
    for(;;) {
        eprintfln("Printed every %lu seconds!", secs);
        gtsleep(secs * 1000);
    }
}
int main(void) {
    gtinit();
    gtgo(printer_thread, (void*)(unsigned long)1);
    gtgo(printer_thread, (void*)(unsigned long)10);
#if 1
    int server = socket(AF_INET, SOCK_STREAM, 0);
    if(server < 0) {
        eprintfln("FATAL: Could not create server socket: %s", sneterr());
        return 1;
    }
    int opt = 1;
    if(setsockopt(server, SOL_SOCKET, SO_REUSEADDR, (void*)&opt, sizeof(opt)) < 0) {
        eprintfln("FATAL: Could not set SO_REUSEADDR: %s", sneterr());
        closesocket(server);
        return 1;
    }
    assert(server >= 0);
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    if(bind(server, (struct sockaddr*)&address, sizeof(address)) < 0) {
        eprintfln("FATAL: Could not bind server: %s", sneterr());
        return 1;
    }
    if(listen(server, 64) < 0) {
        eprintfln("FATAL: Could not listen on server: %s", sneterr());
        return 1;
    }
    eprintfln("INFO: Listening on port 0.0.0.0:%d", PORT);

    for(;;) {
        Client* client = malloc(sizeof(Client));
        if(!client) {
            eprintfln("FATAL: Out of memory allocating client. Dying");
            break;
        }
        struct sockaddr_in client_addr = { 0 };
        unsigned int client_addr_len = sizeof(client_addr);
        gtblockfd(server, GTBLOCKIN);
        int client_fd = accept(server, (void*)&client_addr, &client_addr_len);
        client->fd = client_fd;
        eprintfln("Somebody connected!");
        gtgo(client_thread, client);
    }
    return 0;
#endif
}
