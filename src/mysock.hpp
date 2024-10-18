#pragma once

#include <sstream>
#include <memory>
#include <cstdlib>

#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")

struct Link {
    // TODO: asio
    Link() = default;
    Link(const Link &) = delete;
    Link(Link &&) = delete;
    bool link(const std::string &host, uint32_t port) {
        WORD sockVersion = MAKEWORD(2, 2);
        WSADATA data;
        if (WSAStartup(sockVersion, &data) != 0) {
            return false;
        }
        clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (clientSocket == INVALID_SOCKET) {
            return false;
        }
        sockaddr_in sock_in;
        sock_in.sin_family = AF_INET;
        sock_in.sin_port = htons(port);
        // inet_pton(AF_INET, "127.0.0.1", &sock_in.sin_addr);
        sock_in.sin_addr.S_un.S_addr = inet_addr(host.c_str());
        if (connect(clientSocket, (sockaddr *)&sock_in, sizeof(sock_in)) == SOCKET_ERROR) {
            return false;
        }
        return true;
    }
    bool sendValue(std::string_view v) {
        static char endFlag = '\n';
        if (flag == 1 || v.contains('\n')) {
            return false;
        }
        flag = 1;
        send(clientSocket, v.data(), int(v.size()), 0);
        send(clientSocket, &endFlag, 1, 0);
        return true;
    }
    std::string getValue() {
        if (flag == 2) {
            return "";
        }
        flag = 2;
        int num;
        std::string ret;
        do{
            num = recv(clientSocket, contentBuffer.get(), 1024, 0);
            ret += std::string_view{contentBuffer.get(), size_t(num)};
        }while(contentBuffer[num - 1] != '\n');
        return ret;
    }
    ~Link(){
        closesocket(clientSocket);
    }
    // must send -> get -> send -> get
    int flag = 0;
    std::unique_ptr<char[]> contentBuffer = std::make_unique<char[]>(1024 + 5);
    SOCKET clientSocket = 0;
};