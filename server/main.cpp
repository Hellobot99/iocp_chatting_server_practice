#include "Server.h"
#include <iostream>

int main()
{
    const int iocpThreadCount = 4;
    const int dbThreadCount = 2;

    Server gameServer(iocpThreadCount, dbThreadCount);

    std::cout << "Server starting..." << std::endl;

    if (!gameServer.GetPersistence().Initialize("tcp://127.0.0.1:3306", "root", "1234", "127.0.0.1", 6379))
    {
        std::cerr << "DB Initialization Failed!" << std::endl;
        return -1;
    }

    if (gameServer.Start(9190)) {
        std::cout << "Server is running." << std::endl;

        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    else {
        std::cerr << "Failed to start server." << std::endl;
    }

    return 0;
}