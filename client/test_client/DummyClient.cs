using System;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;
using System.Runtime.InteropServices;
using System.Text;
using System.Collections.Generic;

namespace TestClient
{
    // ==================================================================================
    // [1] 패킷 구조체 정의
    // ==================================================================================

    public enum PacketId : ushort
    {
        REGISTER_REQ = 1, REGISTER_RES = 2,
        LOGIN_REQ = 3, LOGIN_RES = 4,
        ENTER_ROOM = 5, LEAVE_ROOM = 6,
        CHAT = 7, MOVE = 8, SNAPSHOT = 9,
        ROOM_LIST_REQ = 10, ROOM_LIST_RES = 11,
        CREATE_ROOM_REQ = 12, CREATE_ROOM_RES = 13,
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct GameHeader
    {
        public ushort packetSize;
        public ushort packetId;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct PacketRegisterReq
    {
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 50)]
        public byte[] username;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 50)]
        public byte[] password;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct PacketRegisterRes
    {
        [MarshalAs(UnmanagedType.I1)]
        public bool success;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct PacketLoginReq
    {
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 50)]
        public byte[] username;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 50)]
        public byte[] password;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct PacketLoginRes
    {
        [MarshalAs(UnmanagedType.I1)]
        public bool success;
        public uint playerId;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct PacketRoomListRes
    {
        public int count;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct PacketCreateRoomReq
    {
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)]
        public byte[] title;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct PacketCreateRoomRes
    {
        [MarshalAs(UnmanagedType.I1)]
        public bool success;
        public int roomId;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct PacketEnterRoom
    {
        public int roomId;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct PacketMove
    {
        public float vx;
        public float vy;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct PacketChat
    {
        public uint playerId;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 256)]
        public byte[] msg;
    }

    // ==================================================================================
    // [2] 더미 클라이언트 클래스
    // ==================================================================================

    public class DummyClient
    {
        private TcpClient _client;
        private NetworkStream _stream;
        private int _dummyId;
        private bool _isConnected = false;
        private Random _rand = new Random();

        // 상태 확인용 플래그
        private bool _isLoginSuccess = false;
        private bool _loginReqDone = false; // 로그인 응답 수신 여부

        private bool _isRegisterSuccess = false;
        private bool _registerReqDone = false; // 회원가입 응답 수신 여부

        private bool _isInRoom = false;
        private uint _myPlayerId = 0;

        // 시나리오 진행을 위한 데이터
        private int _lastRoomCount = -1;
        private int _createdRoomId = -1;

        public DummyClient(int id)
        {
            _dummyId = id;
        }

        public async Task StartAsync(string ip, int port)
        {
            try
            {
                _client = new TcpClient();
                await _client.ConnectAsync(ip, port);
                _stream = _client.GetStream();
                _isConnected = true;

                Console.WriteLine($"[{_dummyId}] Connected to Server");

                // 1. 패킷 수신 루프 시작 (백그라운드)
                _ = Task.Run(ReceiveLoop);

                // 2. 로그인 시나리오 (실패 시 회원가입)
                await LoginScenario();

                // 3. 방 입장 시나리오 (방 없으면 생성)
                if (_isLoginSuccess)
                {
                    await RoomScenario();
                }

                // 4. 인게임 루프 (이동/채팅)
                if (_isInRoom)
                {
                    await ActionLoop();
                }
            }
            catch (Exception e)
            {
                Console.WriteLine($"[{_dummyId}] Critical Error: {e.Message}");
                Disconnect();
            }
        }

        // -----------------------------------------------------------
        // 시나리오 로직 (수정된 부분)
        // -----------------------------------------------------------

        private async Task LoginScenario()
        {
            string username = $"User_{_dummyId}";
            string password = "1234";

            var userBytes = StringToBytes(username, 50);
            var passBytes = StringToBytes(password, 50);

            // [1단계] 로그인 시도
            Console.WriteLine($"[{_dummyId}] Try Login...");
            PacketLoginReq loginReq = new PacketLoginReq { username = userBytes, password = passBytes };

            _loginReqDone = false;
            SendPacket(PacketId.LOGIN_REQ, loginReq);

            // 응답 대기
            await WaitUntil(() => _loginReqDone, 2000); // 2초 대기

            if (_isLoginSuccess)
            {
                return; // 성공했으면 바로 리턴
            }

            // [2단계] 로그인 실패 -> 회원가입 시도
            Console.WriteLine($"[{_dummyId}] Login Failed. Try Register...");
            PacketRegisterReq regReq = new PacketRegisterReq { username = userBytes, password = passBytes };

            _registerReqDone = false;
            SendPacket(PacketId.REGISTER_REQ, regReq);

            // 회원가입 응답 대기
            await WaitUntil(() => _registerReqDone, 2000);

            if (_isRegisterSuccess)
            {
                Console.WriteLine($"[{_dummyId}] Register Success! Retrying Login...");

                // [3단계] 재로그인
                _loginReqDone = false;
                _isLoginSuccess = false;
                SendPacket(PacketId.LOGIN_REQ, loginReq);

                await WaitUntil(() => _loginReqDone, 2000);
            }
            else
            {
                Console.WriteLine($"[{_dummyId}] Register Failed or Timeout.");
            }
        }

        private async Task RoomScenario()
        {
            // 방 리스트 요청
            SendPacket(PacketId.ROOM_LIST_REQ, new byte[0]);

            // 리스트 응답 대기
            while (_lastRoomCount == -1) await Task.Delay(50);

            if (_lastRoomCount == 0)
            {
                // 방이 없으면 생성
                Console.WriteLine($"[{_dummyId}] No rooms. Creating one...");
                PacketCreateRoomReq createReq = new PacketCreateRoomReq
                {
                    title = StringToBytes($"Room_{_dummyId}", 32)
                };
                SendPacket(PacketId.CREATE_ROOM_REQ, createReq);

                // 생성 응답 대기
                while (_createdRoomId == -1) await Task.Delay(50);

                // 생성된 방 입장
                SendEnterRoom(_createdRoomId);
            }
            else
            {
                // 방이 있으면 1번 방 입장 시도 (테스트용)
                SendEnterRoom(1);
            }

            // 입장 처리 대기
            await Task.Delay(500);
            _isInRoom = true;
            Console.WriteLine($"[{_dummyId}] Entered Room Logic Start.");
        }

        private void SendEnterRoom(int roomId)
        {
            PacketEnterRoom enterReq = new PacketEnterRoom { roomId = roomId };
            SendPacket(PacketId.ENTER_ROOM, enterReq);
        }

        private async Task ActionLoop()
        {
            while (_isConnected)
            {
                try
                {
                    // 80% 이동, 20% 채팅
                    int r = _rand.Next(0, 10);
                    if (r < 8)
                    {
                        PacketMove moveReq = new PacketMove
                        {
                            vx = (float)(_rand.NextDouble() * 2 - 1),
                            vy = (float)(_rand.NextDouble() * 2 - 1)
                        };
                        SendPacket(PacketId.MOVE, moveReq);
                    }
                    else
                    {
                        PacketChat chatReq = new PacketChat
                        {
                            playerId = _myPlayerId,
                            msg = StringToBytes($"Hello from {_dummyId}", 256)
                        };
                        SendPacket(PacketId.CHAT, chatReq);
                    }

                    // 0.2 ~ 1.0초 간격
                    await Task.Delay(_rand.Next(200, 1000));
                }
                catch { break; }
            }
        }

        // -----------------------------------------------------------
        // 네트워크 / 마샬링 헬퍼
        // -----------------------------------------------------------

        private void SendPacket<T>(PacketId id, T payload) where T : struct
        {
            if (!_isConnected) return;

            int headerSize = Marshal.SizeOf(typeof(GameHeader));
            int payloadSize = Marshal.SizeOf(typeof(T));
            int totalSize = headerSize + payloadSize;

            byte[] buffer = new byte[totalSize];

            GameHeader header = new GameHeader
            {
                packetSize = (ushort)totalSize,
                packetId = (ushort)id
            };

            IntPtr ptr = Marshal.AllocHGlobal(totalSize);
            try
            {
                Marshal.StructureToPtr(header, ptr, false);
                Marshal.StructureToPtr(payload, ptr + headerSize, false);
                Marshal.Copy(ptr, buffer, 0, totalSize);
            }
            finally
            {
                Marshal.FreeHGlobal(ptr);
            }

            try
            {
                lock (_stream)
                {
                    _stream.Write(buffer, 0, buffer.Length);
                }
            }
            catch { Disconnect(); }
        }

        private void SendPacket(PacketId id, byte[] emptyPayload)
        {
            if (!_isConnected) return;
            int headerSize = Marshal.SizeOf(typeof(GameHeader));
            byte[] buffer = new byte[headerSize];

            GameHeader header = new GameHeader
            {
                packetSize = (ushort)headerSize,
                packetId = (ushort)id
            };

            IntPtr ptr = Marshal.AllocHGlobal(headerSize);
            try
            {
                Marshal.StructureToPtr(header, ptr, false);
                Marshal.Copy(ptr, buffer, 0, headerSize);
            }
            finally
            {
                Marshal.FreeHGlobal(ptr);
            }

            try
            {
                lock (_stream)
                {
                    _stream.Write(buffer, 0, buffer.Length);
                }
            }
            catch { Disconnect(); }
        }

        private async Task ReceiveLoop()
        {
            int headerSize = Marshal.SizeOf(typeof(GameHeader));
            byte[] headerBuffer = new byte[headerSize];

            while (_isConnected)
            {
                try
                {
                    int bytesRead = await ReadExactAsync(headerBuffer, headerSize);
                    if (bytesRead == 0) { Disconnect(); break; }

                    GameHeader header = BytesToStruct<GameHeader>(headerBuffer);

                    int payloadSize = header.packetSize - headerSize;
                    byte[] payloadBuffer = new byte[payloadSize];

                    if (payloadSize > 0)
                    {
                        await ReadExactAsync(payloadBuffer, payloadSize);
                    }

                    ProcessPacket((PacketId)header.packetId, payloadBuffer);
                }
                catch (Exception)
                {
                    Disconnect();
                    break;
                }
            }
        }

        private async Task<int> ReadExactAsync(byte[] buffer, int size)
        {
            int totalRead = 0;
            while (totalRead < size)
            {
                int read = await _stream.ReadAsync(buffer, totalRead, size - totalRead);
                if (read == 0) return 0;
                totalRead += read;
            }
            return totalRead;
        }

        private void ProcessPacket(PacketId id, byte[] payload)
        {
            switch (id)
            {
                case PacketId.REGISTER_RES:
                    {
                        var res = BytesToStruct<PacketRegisterRes>(payload);
                        _isRegisterSuccess = res.success;
                        _registerReqDone = true;
                        Console.WriteLine($"[{_dummyId}] Register Response: {res.success}");
                    }
                    break;

                case PacketId.LOGIN_RES:
                    {
                        var res = BytesToStruct<PacketLoginRes>(payload);
                        _isLoginSuccess = res.success;
                        if (res.success)
                        {
                            _myPlayerId = res.playerId;
                            Console.WriteLine($"[{_dummyId}] Login Success! ID: {_myPlayerId}");
                        }
                        else
                        {
                            Console.WriteLine($"[{_dummyId}] Login Failed.");
                        }
                        _loginReqDone = true;
                    }
                    break;

                case PacketId.ROOM_LIST_RES:
                    {
                        var res = BytesToStruct<PacketRoomListRes>(payload);
                        _lastRoomCount = res.count;
                        Console.WriteLine($"[{_dummyId}] Room Count Received: {res.count}");
                    }
                    break;

                case PacketId.CREATE_ROOM_RES:
                    {
                        var res = BytesToStruct<PacketCreateRoomRes>(payload);
                        if (res.success)
                        {
                            _createdRoomId = res.roomId;
                            Console.WriteLine($"[{_dummyId}] Room Created. ID: {res.roomId}");
                        }
                    }
                    break;
            }
        }

        // -----------------------------------------------------------
        // 유틸리티
        // -----------------------------------------------------------

        private async Task<bool> WaitUntil(Func<bool> condition, int timeoutMs)
        {
            int totalWaited = 0;
            while (totalWaited < timeoutMs)
            {
                if (condition()) return true;
                await Task.Delay(50);
                totalWaited += 50;
            }
            return false;
        }

        private byte[] StringToBytes(string str, int size)
        {
            byte[] buffer = new byte[size];
            byte[] strBytes = Encoding.UTF8.GetBytes(str);
            Array.Copy(strBytes, buffer, Math.Min(strBytes.Length, size));
            return buffer;
        }

        private T BytesToStruct<T>(byte[] buffer) where T : struct
        {
            int size = Marshal.SizeOf(typeof(T));
            IntPtr ptr = Marshal.AllocHGlobal(size);
            try
            {
                Marshal.Copy(buffer, 0, ptr, size);
                return Marshal.PtrToStructure<T>(ptr);
            }
            finally
            {
                Marshal.FreeHGlobal(ptr);
            }
        }

        private void Disconnect()
        {
            if (!_isConnected) return;
            _isConnected = false;
            _client?.Close();
            Console.WriteLine($"[{_dummyId}] Disconnected");
        }
    }
}