using UnityEngine;
using TMPro;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System;
using System.Runtime.InteropServices;
// [추가] 네임스페이스 추가
using PimDeWitte.UnityMainThreadDispatcher;

public class ChatClient : MonoBehaviour
{
    [Header("UI References")]
    public TMP_InputField inputField;
    public TextMeshProUGUI chatText;
    public UnityEngine.UI.Button sendButton;

    [Header("Connection Settings")]
    public string serverIp = "127.0.0.1";
    public int serverPort = 9190;

    private TcpClient client;
    private NetworkStream stream;
    private Thread receiveThread;
    private bool isConnected = false;

    // [삭제] ConcurrentQueue 불필요 (Dispatcher가 대신함)
    // private ConcurrentQueue<string> messageQueue = new ConcurrentQueue<string>();

    void Start()
    {
        // [체크] Dispatcher가 씬에 있는지 확인 (없으면 경고)
        if (!UnityMainThreadDispatcher.Exists())
        {
            Debug.LogError("씬에 'UnityMainThreadDispatcher' 프리팹이나 컴포넌트가 없습니다! 추가해주세요.");
        }

        ConnectToServer();

        if (inputField != null)
            inputField.onSubmit.AddListener((val) => SendMessageToServer());

        if (sendButton != null)
            sendButton.onClick.AddListener(SendMessageToServer);
    }

    // [삭제] Update() 함수 불필요
    /*
    void Update()
    {
        while (messageQueue.TryDequeue(out string msg)) { ... }
    }
    */

    void ConnectToServer()
    {
        try
        {
            client = new TcpClient(serverIp, serverPort);
            stream = client.GetStream();
            isConnected = true;

            receiveThread = new Thread(ReceiveLoop);
            receiveThread.IsBackground = true;
            receiveThread.Start();

            Debug.Log($"[ChatClient] 서버({serverIp}:{serverPort}) 연결 성공!");

            // [변경] 메인 스레드 디스패처로 UI 업데이트
            UnityMainThreadDispatcher.Instance().Enqueue(() => {
                AddSystemMessage("서버에 연결되었습니다.", Color.green);
            });
        }
        catch (Exception e)
        {
            Debug.LogError($"[ChatClient] 연결 실패: {e.Message}");
            UnityMainThreadDispatcher.Instance().Enqueue(() => {
                AddSystemMessage($"서버 연결 실패. (Port: {serverPort})", Color.red);
            });
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

                ProcessPacket((PacketId)header.packetId, bodyBuffer);
            }
        }
        catch (Exception e)
        {
            if (isConnected)
            {
                Debug.LogError($"수신 오류: {e.Message}");
                // [변경] 연결 끊김 알림
                UnityMainThreadDispatcher.Instance().Enqueue(() => {
                    AddSystemMessage("서버와의 연결이 끊어졌습니다.", Color.red);
                });
            }
        }
        finally
        {
            CloseConnection();
        }
    }

    void ProcessPacket(PacketId id, byte[] data)
    {
        switch (id)
        {
            case PacketId.CHAT:
                {
                    PacketChat pkt = ByteArrayToStructure<PacketChat>(data);
                    string msg = Encoding.UTF8.GetString(pkt.msg).TrimEnd('\0');

                    // [핵심 변경] 수신 스레드에서 -> 메인 스레드로 UI 작업 넘김
                    UnityMainThreadDispatcher.Instance().Enqueue(() => {
                        chatText.text += $"{msg}\n";

                        // 텍스트 길이 관리
                        if (chatText.text.Length > 5000)
                            chatText.text = chatText.text.Substring(2000);
                    });
                }
                break;
        }
    }

    // [헬퍼] 시스템 메시지 출력용
    void AddSystemMessage(string msg, Color color)
    {
        string colorHex = ColorUtility.ToHtmlStringRGB(color);
        chatText.text += $"<color=#{colorHex}>{msg}</color>\n";
    }

    // ... (SendMessageToServer, ReadExact, CloseConnection, 구조체 정의 등은 기존과 동일) ...
    // ... 아래 내용은 기존 코드 유지 ...

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

    public void SendMessageToServer()
    {
        if (!isConnected || stream == null) return;
        if (string.IsNullOrWhiteSpace(inputField.text)) return;

        try
        {
            PacketChat chatPkt = new PacketChat();
            chatPkt.playerId = 0;
            chatPkt.msg = new byte[256];

            byte[] strBytes = Encoding.UTF8.GetBytes(inputField.text);
            int copyLen = Math.Min(strBytes.Length, 256);
            Array.Copy(strBytes, chatPkt.msg, copyLen);

            byte[] bodyData = StructureToByteArray(chatPkt);

            GameHeader header = new GameHeader();
            header.packetId = (ushort)PacketId.CHAT;
            header.packetSize = (ushort)(Marshal.SizeOf(typeof(GameHeader)) + bodyData.Length);

            byte[] headerData = StructureToByteArray(header);
            byte[] packetData = new byte[header.packetSize];
            Array.Copy(headerData, 0, packetData, 0, headerData.Length);
            Array.Copy(bodyData, 0, packetData, headerData.Length, bodyData.Length);

            stream.Write(packetData, 0, packetData.Length);

            inputField.text = "";
            inputField.ActivateInputField();
        }
        catch (Exception e)
        {
            Debug.LogError($"전송 오류: {e.Message}");
            CloseConnection();
        }
    }

    void CloseConnection()
    {
        isConnected = false;
        if (stream != null) stream.Close();
        if (client != null) client.Close();
    }

    void OnApplicationQuit()
    {
        CloseConnection();
    }

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
        IntPtr ptr = Marshal.AllocHGlobal(len);
        Marshal.Copy(bytearray, 0, ptr, len);
        str = (T)Marshal.PtrToStructure(ptr, typeof(T));
        Marshal.FreeHGlobal(ptr);
        return str;
    }
}

// [패킷 구조체 정의는 이전과 동일]
public enum PacketId : ushort { LOGIN = 1, CHAT = 2, MOVE = 3, SNAPSHOT = 4 }

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct GameHeader { public ushort packetSize; public ushort packetId; }

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct PacketChat { public uint playerId; [MarshalAs(UnmanagedType.ByValArray, SizeConst = 256)] public byte[] msg; }