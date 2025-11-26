using System;
using System.Net;
using System.Net.Sockets;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;

namespace ChatClient
{
    // ==================================================================================
    // [프로토콜 정의] 서버의 NetProtocol.h와 구조가 100% 일치해야 함 (Pack = 1 중요)
    // ==================================================================================

    public enum PacketId : ushort
    {
        LOGIN = 1,
        CHAT = 2,
        MOVE = 3,
        SNAPSHOT = 4,
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct GameHeader
    {
        public ushort packetSize;
        public ushort packetId;
    }

    // 서버의 PacketChat 구조체와 대응 (playerId(4) + msg(256))
    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct PacketChat
    {
        public uint playerId;

        // 고정 길이 배열 (char msg[256])
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 256)]
        public byte[] msg;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct PacketLogin
    {
        public bool loginOk;
        public ulong playerId;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)]
        public byte[] nickname;
    }

    // ==================================================================================
    // [클라이언트 메인 로직]
    // ==================================================================================

    class Program
    {
        // 서버 설정 (환경에 맞게 수정하세요)
        const string SERVER_IP = "127.0.0.1";
        const int SERVER_PORT = 9190;

        static Socket _socket;
        static bool _isConnected = false;

        static void Main(string[] args)
        {
            Console.Title = "C# Chat Client (Struct Packet)";
            Console.WriteLine($"[Client] Connecting to {SERVER_IP}:{SERVER_PORT}...");

            try
            {
                // 1. 소켓 연결
                _socket = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
                IPEndPoint endPoint = new IPEndPoint(IPAddress.Parse(SERVER_IP), SERVER_PORT);
                _socket.Connect(endPoint);

                _isConnected = true;
                Console.WriteLine($"[Client] Connected! Local: {_socket.LocalEndPoint}");

                // 2. 수신 스레드 시작
                Thread recvThread = new Thread(ReceiveThread);
                recvThread.IsBackground = true;
                recvThread.Start();

                // 3. (옵션) 로그인 패킷 전송 예시
                // SendLoginPacket("Guest"); 

                // 4. 입력 루프 (채팅 전송)
                Console.WriteLine("[Info] Type message and press Enter to send.");
                while (_isConnected)
                {
                    string input = Console.ReadLine();
                    if (string.IsNullOrEmpty(input)) continue;

                    SendChatPacket(input);
                }
            }
            catch (Exception e)
            {
                Console.WriteLine($"[Error] {e.Message}");
            }
        }

        static void SendChatPacket(string message)
        {
            if (!_isConnected) return;

            // 1. 구조체 채우기
            PacketChat pkt = new PacketChat();
            pkt.playerId = 0; // 클라가 보낼 땐 보통 0 혹은 내 ID
            pkt.msg = new byte[256];

            // 문자열 -> UTF8 바이트 변환 및 복사
            byte[] msgBytes = Encoding.UTF8.GetBytes(message);
            int copyLen = Math.Min(msgBytes.Length, 256);
            Array.Copy(msgBytes, pkt.msg, copyLen);

            // 2. 직렬화 (구조체 -> 바이트 배열)
            byte[] bodyData = StructureToByteArray(pkt);

            // 3. 헤더 설정
            GameHeader header = new GameHeader();
            header.packetSize = (ushort)(Marshal.SizeOf(typeof(GameHeader)) + bodyData.Length);
            header.packetId = (ushort)PacketId.CHAT;

            byte[] headerData = StructureToByteArray(header);

            // 4. 전송 (Header + Body)
            // 성능을 위해 하나의 버퍼로 합쳐서 보내는 것이 좋음
            byte[] sendBuffer = new byte[header.packetSize];
            Array.Copy(headerData, 0, sendBuffer, 0, headerData.Length);
            Array.Copy(bodyData, 0, sendBuffer, headerData.Length, bodyData.Length);

            _socket.Send(sendBuffer);
        }

        static void ReceiveThread()
        {
            byte[] headerBuffer = new byte[Marshal.SizeOf(typeof(GameHeader))];

            while (_isConnected)
            {
                try
                {
                    // 1. 헤더 수신 (4바이트)
                    int received = _socket.Receive(headerBuffer, 0, headerBuffer.Length, SocketFlags.None);
                    if (received == 0) break; // 연결 끊김

                    if (received < headerBuffer.Length) continue; // 헤더가 덜 왔으면 대기 (간략화됨)

                    GameHeader header = ByteArrayToStructure<GameHeader>(headerBuffer);

                    // 2. 바디 수신
                    int bodySize = header.packetSize - Marshal.SizeOf(typeof(GameHeader));
                    byte[] bodyBuffer = new byte[bodySize];

                    int totalBodyReceived = 0;
                    while (totalBodyReceived < bodySize)
                    {
                        int recv = _socket.Receive(bodyBuffer, totalBodyReceived, bodySize - totalBodyReceived, SocketFlags.None);
                        if (recv == 0) return;
                        totalBodyReceived += recv;
                    }

                    // 3. 패킷 처리
                    ProcessPacket((PacketId)header.packetId, bodyBuffer);
                }
                catch (SocketException)
                {
                    Console.WriteLine("[Client] Disconnected from server.");
                    _isConnected = false;
                    break;
                }
            }
        }

        static void ProcessPacket(PacketId id, byte[] data)
        {
            switch (id)
            {
                case PacketId.CHAT:
                    {
                        PacketChat pkt = ByteArrayToStructure<PacketChat>(data);
                        // C++ char[] 문자열(UTF8) 디코딩 (Null 문자 제거)
                        string msg = Encoding.UTF8.GetString(pkt.msg).TrimEnd('\0');
                        Console.WriteLine($"[Chat] User({pkt.playerId}): {msg}");
                    }
                    break;

                case PacketId.LOGIN:
                    {
                        Console.WriteLine("[Recv] Login Packet Received");
                    }
                    break;

                // 스냅샷 등 다른 패킷 처리 추가
                default:
                    Console.WriteLine($"[Recv] Unknown Packet ID: {id}");
                    break;
            }
        }

        // ==================================================================================
        // [헬퍼 함수] 구조체 <-> 바이트 배열 변환 (Marshalling)
        // ==================================================================================

        static byte[] StructureToByteArray(object obj)
        {
            int size = Marshal.SizeOf(obj);
            byte[] arr = new byte[size];
            IntPtr ptr = Marshal.AllocHGlobal(size);

            Marshal.StructureToPtr(obj, ptr, true);
            Marshal.Copy(ptr, arr, 0, size);
            Marshal.FreeHGlobal(ptr);

            return arr;
        }

        static T ByteArrayToStructure<T>(byte[] bytes) where T : struct
        {
            T str = default(T);
            int size = Marshal.SizeOf(str);
            IntPtr ptr = Marshal.AllocHGlobal(size);

            Marshal.Copy(bytes, 0, ptr, size);
            str = (T)Marshal.PtrToStructure(ptr, typeof(T));
            Marshal.FreeHGlobal(ptr);

            return str;
        }
    }
}