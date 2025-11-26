#include <iostream>
#include "Server.h"

int main()
{
    const int iocpThreadCount = 4;
    const int dbThreadCount = 2;

    Server gameServer(iocpThreadCount, dbThreadCount);

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
                break; // 루프 탈출 -> main 함수 종료 -> gameServer 소멸자 호출 -> Stop() 실행
            }

            // 필요하다면 여기에 "kick 10" 같은 관리자 명령어 처리 로직을 넣을 수도 있음
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    else {
        std::cerr << "Failed to start server." << std::endl;
    }

    return 0;
}