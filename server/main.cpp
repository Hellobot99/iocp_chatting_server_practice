#include <iostream>
#include "Server.h"

Server* g_Server = nullptr;

int main()
{
    const int iocpThreadCount = 4;
    const int dbThreadCount = 2;

    Server gameServer(iocpThreadCount, dbThreadCount);
    g_Server = &gameServer;

    std::cout << "Server starting..." << std::endl;

    if (!gameServer.GetPersistence().Initialize("tcp://127.0.0.1:3306", "root", "1234"))
    {
        std::cerr << "DB Initialization Failed!" << std::endl;
        return -1;
    }

    if (gameServer.Start(9190)) {
        std::cout << "Server is running." << std::endl;

        while (true) {
            std::string command;
            std::cin >> command;

            if (command == "quit") {
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    else {
        std::cerr << "Failed to start server." << std::endl;
    }

    return 0;
}