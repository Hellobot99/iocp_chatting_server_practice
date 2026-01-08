using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;

namespace TestClient
{
    class Program
    {
        static void Main(string[] args)
        {
            Console.WriteLine("Starting Test Clients...");

            string serverIp = "127.0.0.1"; // 서버 IP
            int serverPort = 9190;         // 서버 포트
            int clientCount = 1;         // 접속시킬 클라이언트 수

            List<Task> clients = new List<Task>();

            for (int i = 0; i < clientCount; i++)
            {
                int id = i;
                // 비동기로 클라이언트 시작
                clients.Add(Task.Run(() =>
                {
                    DummyClient client = new DummyClient(id);
                    return client.StartAsync(serverIp, serverPort);
                }));

                // 한 번에 너무 많이 접속하면 서버 Accept 큐가 터질 수 있으므로 약간의 텀을 둠
                if (i % 10 == 0) Thread.Sleep(10);
            }

            Console.WriteLine($"{clientCount} clients spawned. Press any key to exit.");
            Console.ReadKey();
        }
    }
}