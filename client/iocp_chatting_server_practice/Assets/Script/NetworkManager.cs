using UnityEngine;
using System.Net.Sockets;
using System.Threading;
using System;
using System.Runtime.InteropServices;
using PimDeWitte.UnityMainThreadDispatcher;

public class NetworkManager : MonoBehaviour
{
    public static NetworkManager Instance;

    [Header("Connection Settings")]
    public string serverIp = "127.0.0.1";
    public int serverPort = 9190;

    private TcpClient client;
    private NetworkStream stream;
    private Thread receiveThread;
    private bool isConnected = false;

    public uint MyPlayerId { get; set; } // 로그인 성공 시 할당

    void Awake()
    {
        if (Instance == null)
        {
            Instance = this;
            DontDestroyOnLoad(gameObject); // 씬 넘어가도 파괴 X
        }
        else
        {
            Destroy(gameObject);
        }
    }

    void Start()
    {
       
    }

    public void ConnectToServer()
    {
        try
        {
            client = new TcpClient(serverIp, serverPort);
            stream = client.GetStream();
            isConnected = true;

            receiveThread = new Thread(ReceiveLoop);
            receiveThread.IsBackground = true;
            receiveThread.Start();

            Debug.Log($"[Network] Connected to {serverIp}:{serverPort}");

            // 연결 성공 시 UI에 메시지 띄우라고 ChatUI에 요청할 수도 있음 (선택)
            UnityMainThreadDispatcher.Instance().Enqueue(() => {
                ChatUI.Instance?.AddSystemMessage("서버에 연결되었습니다.", Color.green);
            });
        }
        catch (Exception e)
        {
            Debug.LogError($"[Network] Connection Failed: {e.Message}");
        }
    }

    // 외부(GameManager, ChatUI)에서 패킷 보낼 때 사용하는 함수
    public void SendPacket(PacketId id, object packetStruct)
    {
        if (!isConnected) return;

        try
        {
            byte[] bodyData;

            // [수정 핵심] 입력된 값이 '바이트 배열'이면 변환 없이 그대로 사용
            if (packetStruct is byte[])
            {
                bodyData = (byte[])packetStruct;
            }
            else
            {
                // 입력된 값이 '구조체'면 바이트로 변환 (기존 로직)
                bodyData = StructureToByteArray(packetStruct);
            }

            // 헤더 생성
            GameHeader header = new GameHeader();
            header.packetId = (ushort)id;
            header.packetSize = (ushort)(Marshal.SizeOf(typeof(GameHeader)) + bodyData.Length);

            byte[] headerData = StructureToByteArray(header);

            // 최종 패킷 합치기 (헤더 + 바디)
            byte[] packetData = new byte[header.packetSize];
            Array.Copy(headerData, 0, packetData, 0, headerData.Length);
            Array.Copy(bodyData, 0, packetData, headerData.Length, bodyData.Length);

            lock (stream)
            {
                stream.Write(packetData, 0, packetData.Length);
            }
        }
        catch (Exception e)
        {
            Debug.LogError($"Send Error: {e.Message}");
            CloseConnection();
        }
    }

    void ReceiveLoop()
    {
        int headerSize = Marshal.SizeOf(typeof(GameHeader));
        byte[] headerBuffer = new byte[headerSize];

        try
        {
            while (isConnected)
            {
                if (!ReadExact(headerBuffer, headerSize)) break;
                GameHeader header = ByteArrayToStructure<GameHeader>(headerBuffer);

                int bodySize = header.packetSize - headerSize;
                if (bodySize < 0) continue;

                byte[] bodyBuffer = new byte[bodySize];
                if (!ReadExact(bodyBuffer, bodySize)) break;

                // [중요] 패킷 처리를 핸들러에게 위임
                PacketManager.Instance.ProcessPacket((PacketId)header.packetId, bodyBuffer);
            }
        }
        catch (Exception e)
        {
            if (isConnected) Debug.LogWarning($"Recv Error: {e.Message}");
        }
        finally
        {
            CloseConnection();
        }
    }

    bool ReadExact(byte[] buffer, int size)
    {
        int totalRead = 0;
        while (totalRead < size)
        {
            int read = stream.Read(buffer, totalRead, size - totalRead);
            if (read <= 0) return false;
            totalRead += read;
        }
        return true;
    }

    public void CloseConnection()
    {
        isConnected = false;
        if (stream != null) stream.Close();
        if (client != null) client.Close();
    }

    void OnApplicationQuit()
    {
        if (isConnected)
        {
            SendLogout();

            Thread.Sleep(100);

            CloseConnection();
        }
    }

    // 유틸리티 함수들
    public static byte[] StructureToByteArray(object obj)
    {
        int len = Marshal.SizeOf(obj);
        byte[] arr = new byte[len];
        IntPtr ptr = Marshal.AllocHGlobal(len);
        Marshal.StructureToPtr(obj, ptr, true);
        Marshal.Copy(ptr, arr, 0, len);
        Marshal.FreeHGlobal(ptr);
        return arr;
    }

    public static T ByteArrayToStructure<T>(byte[] bytearray) where T : struct
    {
        T str = default(T);
        int len = Marshal.SizeOf(str);
        if (len > bytearray.Length) return default(T);
        IntPtr ptr = Marshal.AllocHGlobal(len);
        Marshal.Copy(bytearray, 0, ptr, len);
        str = (T)Marshal.PtrToStructure(ptr, typeof(T));
        Marshal.FreeHGlobal(ptr);
        return str;
    }

    // -------------------------------------------------------------
    // [1] 문자열 변환 헬퍼 (반복되는 코드를 줄이기 위해 추가)
    // -------------------------------------------------------------
    private byte[] ToBytes(string str, int size)
    {
        byte[] buffer = new byte[size];
        if (string.IsNullOrEmpty(str)) return buffer;

        byte[] strBytes = System.Text.Encoding.UTF8.GetBytes(str);
        int copyLen = Math.Min(strBytes.Length, size); // 마지막 null 문자 고려 안 해도 됨 (C++이 알아서 함) or size-1
        Array.Copy(strBytes, 0, buffer, 0, copyLen);

        return buffer;
    }

    // -------------------------------------------------------------
    // [2] 회원가입 요청
    // -------------------------------------------------------------
    public void SendRegister(string username, string password)
    {
        PacketRegisterReq packet = new PacketRegisterReq();

        // 헬퍼 함수를 써서 깔끔하게 할당 (SizeConst = 50)
        packet.username = ToBytes(username, 50);
        packet.password = ToBytes(password, 50);

        SendPacket(PacketId.REGISTER_REQ, packet);
        Debug.Log($"[Send] Register Request: {username}");
    }

    // -------------------------------------------------------------
    // [3] 로그인 요청 (방 번호 없이 아이디/비번만)
    // -------------------------------------------------------------
    public void SendLogin(string username, string password)
    {
        PacketLoginReq packet = new PacketLoginReq();

        packet.username = ToBytes(username, 50);
        packet.password = ToBytes(password, 50);

        SendPacket(PacketId.LOGIN_REQ, packet);
        Debug.Log($"[Send] Login Request: {username}");
    }

    // -------------------------------------------------------------
    // [4] 방 입장 요청 (로그인 성공 후 호출)
    // -------------------------------------------------------------
    public void SendEnterRoom(int roomId)
    {
        PacketEnterRoom packet = new PacketEnterRoom();
        packet.roomId = roomId;

        SendPacket(PacketId.ENTER_ROOM, packet);
        Debug.Log($"[Send] Enter Room Request: {roomId}");
    }

    public void SendChat(string text)
    {
        PacketChat packet = new PacketChat();
        packet.playerId = this.MyPlayerId;

        packet.msg = ToBytes(text, 256);

        SendPacket(PacketId.CHAT, packet);
    }

    public void SendCreateRoom(string title)
    {
        PacketCreateRoomReq packet = new PacketCreateRoomReq();
        packet.title = ToBytes(title, 32); // 기존에 만든 헬퍼 함수 사용

        SendPacket(PacketId.CREATE_ROOM_REQ, packet);
    }

    public void SendLogout()
    {
        if (!isConnected) return;

        // 바디 없이 헤더만 보내면 되므로 빈 바이트 배열 전송
        SendPacket(PacketId.LOGOUT_REQ, new byte[0]);
        Debug.Log("[Network] 로그아웃 요청 전송함");
    }

}