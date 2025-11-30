using UnityEngine;
using TMPro;

public class ChatUI : MonoBehaviour
{
    public static ChatUI Instance;

    [Header("UI References")]
    public TMP_InputField inputField;
    public TextMeshProUGUI chatText;
    public UnityEngine.UI.Button sendButton;

    void Awake()
    {
        Instance = this;
    }

    void Start()
    {
        if (sendButton != null)
            sendButton.onClick.AddListener(SendMessage);

        if (inputField != null)
            inputField.onSubmit.AddListener((val) => SendMessage());
    }

    // [UI -> Network] 채팅 전송
    public void SendMessage()
    {
        if (string.IsNullOrWhiteSpace(inputField.text)) return;
        if (NetworkManager.Instance == null) return;

        // 패킷 생성
        PacketChat chatPkt = new PacketChat();
        chatPkt.playerId = NetworkManager.Instance.MyPlayerId;
        chatPkt.msg = new byte[256];

        byte[] strBytes = System.Text.Encoding.UTF8.GetBytes(inputField.text);
        int copyLen = System.Math.Min(strBytes.Length, 256);
        System.Array.Copy(strBytes, chatPkt.msg, copyLen);

        // NetworkManager에게 전송 위임
        NetworkManager.Instance.SendPacket(PacketId.CHAT, chatPkt);

        inputField.text = "";
        inputField.ActivateInputField();
    }

    // [Network -> UI] 메시지 표시 (PacketHandler에서 호출)
    public void AddChatMessage(string msg)
    {
        chatText.text += $"{msg}\n";

        // 텍스트 길이 제한
        if (chatText.text.Length > 5000)
            chatText.text = chatText.text.Substring(2000);
    }

    public void AddSystemMessage(string msg, Color color)
    {
        string colorHex = ColorUtility.ToHtmlStringRGB(color);
        chatText.text += $"<color=#{colorHex}>{msg}</color>\n";
    }
}