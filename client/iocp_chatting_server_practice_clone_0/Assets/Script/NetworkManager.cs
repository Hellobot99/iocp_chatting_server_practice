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
            byte[] bodyData = StructureToByteArray(packetStruct);

            GameHeader header = new GameHeader();
            header.packetId = (ushort)id;
            header.packetSize = (ushort)(Marshal.SizeOf(typeof(GameHeader)) + bodyData.Length);

            byte[] headerData = StructureToByteArray(header);
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

    public void SendChat(string text)
    {
        PacketChat packet = new PacketChat();

        // 내 플레이어 ID 할당 (필요한 경우)
        packet.playerId = this.MyPlayerId;

        packet.msg = new byte[256];

        // 인코딩 및 복사
        byte[] textBytes = System.Text.Encoding.UTF8.GetBytes(text);
        int copyLen = Math.Min(textBytes.Length, 256 - 1);
        Array.Copy(textBytes, 0, packet.msg, 0, copyLen);

        // 전송
        SendPacket(PacketId.CHAT, packet);
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
        CloseConnection();
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
    public void SendLogin(int roomId, string name)
    {
        PacketLogin packet = new PacketLogin();

        packet.roomId = roomId;     // 방 번호 넣기
        packet.name = new byte[32]; // 이름 공간

        // 1. 이름 변환 및 복사 (기존과 동일)
        byte[] nameBytes = System.Text.Encoding.UTF8.GetBytes(name);
        int copyLen = Math.Min(nameBytes.Length, 32 - 1);
        Array.Copy(nameBytes, 0, packet.name, 0, copyLen);

        // 2. 구조체를 바이트 배열로 직렬화해서 전송
        // (구조체에 int가 섞여있으므로 StructureToByteArray 함수 활용 추천)
        // 만약 수동으로 한다면 아래처럼 해야 함:

        // [간편한 방법] 이미 정의된 StructureToByteArray 사용
        SendPacket(PacketId.LOGIN, packet);
    }
}