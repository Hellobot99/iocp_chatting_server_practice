using System;
using System.Net.Sockets;
using System.Text;
using Google.Protobuf;
using GameServer.Protocol;

namespace DummyClient
{
    class Program
    {
        const string SERVER_IP = "127.0.0.1";
        const int SERVER_PORT = 9190;

        static void Main(string[] args)
        {
            try
            {
                // 1. 서버 연결
                TcpClient client = new TcpClient();
                client.Connect(SERVER_IP, SERVER_PORT);
                Console.WriteLine($"[Client] 서버({SERVER_IP}:{SERVER_PORT}) 접속 성공!");

                NetworkStream stream = client.GetStream();

                // =================================================
                // 2. 패킷 생성 (C_LOGIN)
                // =================================================
                C_Login loginPkt = new C_Login();
                loginPkt.UniqueId = "TestUser_Console_01";

                // 3. Protobuf 직렬화 (Payload)
                byte[] bodyData = loginPkt.ToByteArray();

                // 4. 헤더 생성 (Size + ID)
                // C++ 서버 헤더 구조: [PacketSize(2)][PacketId(2)]
                ushort size = (ushort)(4 + bodyData.Length); // 헤더 크기(4) 포함
                ushort packetId = (ushort)PacketId.CLogin;   // Enum 값 (1)

                byte[] header = new byte[4];
                BitConverter.GetBytes(size).CopyTo(header, 0);
                BitConverter.GetBytes(packetId).CopyTo(header, 2);

                // 5. 전송 (헤더 + 바디)
                stream.Write(header, 0, header.Length);
                stream.Write(bodyData, 0, bodyData.Length);

                Console.WriteLine($"[Client] C_LOGIN 전송 완료 (Size:{size}, ID:{packetId})");

                // =================================================
                // 6. 채팅 패킷도 한번 보내보기 (C_CHAT)
                // =================================================
                C_Chat chatPkt = new C_Chat();
                chatPkt.Username = "TestUser_Console_01";
                chatPkt.Message = "Hello Server! I am C# Console.";

                byte[] chatBody = chatPkt.ToByteArray();
                ushort chatSize = (ushort)(4 + chatBody.Length);
                ushort chatId = (ushort)PacketId.CChat;

                byte[] chatHeader = new byte[4];
                BitConverter.GetBytes(chatSize).CopyTo(chatHeader, 0);
                BitConverter.GetBytes(chatId).CopyTo(chatHeader, 2);

                // 약간 텀을 두고 전송
                System.Threading.Thread.Sleep(100);
                stream.Write(chatHeader, 0, chatHeader.Length);
                stream.Write(chatBody, 0, chatBody.Length);

                Console.WriteLine($"[Client] C_CHAT 전송 완료");

                // 서버가 끊을 때까지 잠시 대기
                System.Threading.Thread.Sleep(5000);

                client.Close();
            }
            catch (Exception e)
            {
                Console.WriteLine($"[Error] {e.Message}");
            }
        }
    }
}