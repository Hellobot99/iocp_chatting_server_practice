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

    public void SendMessage()
    {
        // 1. 예외 처리
        if (string.IsNullOrWhiteSpace(inputField.text)) return;
        if (NetworkManager.Instance == null) return;

        // 2. 복잡한 로직 없이 NetworkManager의 함수 호출
        // (인코딩, 패킷 생성은 저 함수 안에서 다 알아서 함)
        NetworkManager.Instance.SendChat(inputField.text);

        // 3. UI 초기화
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