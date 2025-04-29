#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
constexpr int BUF_LEN = 1024;

int main(int argc, char **argv) {
    std::string requests = "GET /echo/abcxyz";
    char *path = nullptr;
    path = strtok(requests.data(), " ");
    std::cout << "Path: " << path << std::endl;
    path = strtok(NULL, " ");
    std::cout << "Path: " << path << std::endl;

    // delete []path;
}