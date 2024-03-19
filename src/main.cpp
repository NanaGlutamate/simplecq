#include <format>
#include <iostream>
#include <string>

#include <taskflow/taskflow.hpp>

#include "csmodel_base.h"
#include "thread_pool.hpp"
#include "mysock.hpp"

int main() {
    using namespace std;
    WORD sockVersion = MAKEWORD(2, 2);
    WSADATA data;
    if (WSAStartup(sockVersion, &data) != 0)
    {
        return 1;
    }

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET){
        std::cout << "Socket error" << std::endl;
        return 1;
    }
    sockaddr_in sock_in;
    sock_in.sin_family = AF_INET;
    sock_in.sin_port = htons(50023);
    // inet_pton(AF_INET, "127.0.0.1", &sock_in.sin_addr);
    sock_in.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
    if (connect(clientSocket, (sockaddr*)&sock_in, sizeof(sock_in) )== SOCKET_ERROR){
        cout << "Connect error" << endl;
        return 1;
    }

    // string _data;
    // getline(cin, _data);
    // send(clientSocket, _data.c_str(), _data.size(), 0);

    char revdata[100];
    int num = recv(clientSocket, revdata, 100, 0);
    if (num > 0){
        revdata[num] = '\0';
        cout <<"Sever say:"<< revdata << endl;
    }
    closesocket(clientSocket);

    WSACleanup();

    // using namespace cq;
    // tf::Executor executor;
    // tf::Taskflow taskflow;
    // auto [A, B, C, D] = taskflow.emplace( // create four tasks
    //     [] { std::cout << "TaskA\n"
    //                    << std::endl; }, [] { std::cout << "TaskB\n"
    //                                                                 << std::endl; },
    //     [] { std::cout << "TaskC\n"
    //                    << std::endl; }, [] { std::cout << "TaskD\n"
    //                                                                 << std::endl; });
    // A.precede(B, C); // A runs before B and C
    // D.succeed(B, C); // D runs after  B and C
    // executor.run(taskflow).wait();

    // std::cout << std::format("[main]: {1}", std::this_thread::get_id(), "post") << std::endl;
    // {
    //     std::mutex mtx;
    //     ThreadPool tp{8};
    //     for (size_t i = 0; i < 10; i++) {
    //         tp.post([&] {
    //             for (auto i = 0; i < 10000; ++i) {
    //                 {
    //                     std::unique_lock lock{mtx};
    //                     std::cout << std::format("[{}]: {}", std::this_thread::get_id(), i) << std::endl;
    //                 }
    //             }
    //         });
    //     }
    //     // std::cout << std::format("[main]: {1}", std::this_thread::get_id(), "posted and wait") << std::endl;
    //     tp.waitForEmpty();
    //     // std::cout << std::format("[main]: {1}", std::this_thread::get_id(), "done") << std::endl;
    // }
    // std::cout << std::format("[main]: {1}", std::this_thread::get_id(), "end") << std::endl;

    return 0;
}