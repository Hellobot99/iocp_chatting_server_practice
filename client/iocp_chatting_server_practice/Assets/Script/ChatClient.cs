using UnityEngine;
using TMPro;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Collections.Concurrent;
using System;

public class ChatClient : MonoBehaviour
{
    public TMP_InputField inputField;
    public TextMeshProUGUI chatText;

    private TcpClient client;
    private NetworkStream stream;
    private Thread receiveThread;
    private ConcurrentQueue<string> messageQueue = new ConcurrentQueue<string>();

    public string username = "unityUser"; // 유저 이름 설정

    const int PACKET_SIZE = 292; // C++ ChatPacket 크기

    void Start()
    {
        Connect("127.0.0.1", 9190);
    }

    void Update()
    {
        while (messageQueue.TryDequeue(out string msg))
        {
            chatText.text += msg + "\n";
        }
    }

    void Connect(string ip, int port)
    {
        client = new TcpClient(ip, port);
        stream = client.GetStream();

        receiveThread = new Thread(ReceiveLoop);
        receiveThread.Start();
    }

    void ReceiveLoop()
    {
        byte[] buffer = new byte[PACKET_SIZE];
        try
        {
            while (true)
            {
                int bytesRead = stream.Read(buffer, 0, buffer.Length);
                if (bytesRead <= 0) continue;

                // 패킷 파싱
                int packetType = BitConverter.ToInt32(buffer, 0);
                string sender = Encoding.UTF8.GetString(buffer, 4, 32).TrimEnd('\0');
                string message = Encoding.UTF8.GetString(buffer, 36, 256).TrimEnd('\0');

                messageQueue.Enqueue($"[{sender}] {message}");
            }
        }
        catch (Exception e)
        {
            Debug.LogError("수신 오류: " + e.Message);
        }
    }

    public void SendMessageToServer()
    {
        if (string.IsNullOrWhiteSpace(inputField.text)) return;

        byte[] buffer = new byte[PACKET_SIZE];

        // 1) packetType (int, 4바이트)
        BitConverter.GetBytes(0).CopyTo(buffer, 0); // 채팅=0

        // 2) username (32바이트, null padding)
        byte[] nameBytes = Encoding.UTF8.GetBytes(username);
        Array.Copy(nameBytes, 0, buffer, 4, Mathf.Min(nameBytes.Length, 32));

        // 3) message (256바이트, null padding)
        byte[] msgBytes = Encoding.UTF8.GetBytes(inputField.text);
        Array.Copy(msgBytes, 0, buffer, 36, Mathf.Min(msgBytes.Length, 256));

        // 서버로 전송
        stream.Write(buffer, 0, buffer.Length);

        inputField.text = "";
        inputField.ActivateInputField();
    }

    void OnApplicationQuit()
    {
        try
        {
            receiveThread?.Abort();
            stream?.Close();
            client?.Close();
        }
        catch { }
    }
}
