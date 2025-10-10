#include "DBWorker.h"

DWORD WINAPI DBWorkerThread(LPVOID arg)
{
    // 1. LPVOID arg로부터 자신의 인덱스(순번)를 받습니다.
    int threadIndex = reinterpret_cast<intptr_t>(arg);

    // 2. 이 인덱스를 이 스레드만의 TLS 변수에 저장합니다.
    g_current_worker_index = threadIndex;

    // 3. 이 시점부터 이 스레드는 자신의 고유 인덱스를 가지게 됩니다.
    while (true) {
        //... PersistenceRequest 처리 로직...
    }
    return 0;
}