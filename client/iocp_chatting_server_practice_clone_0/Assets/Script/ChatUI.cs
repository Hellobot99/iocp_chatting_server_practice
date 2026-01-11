using UnityEngine;
using TMPro;
using UnityEngine.UI;
using System.Collections;

public class ChatUI : MonoBehaviour
{
    public static ChatUI Instance;

    [Header("UI References")]
    public ScrollRect scrollRect;        // [필수] Scroll View
    public Transform contentTransform;   // [필수] Content
    public GameObject textPrefab;        // [필수] 채팅 텍스트 프리팹 (Text TMP 하나만 있는 프리팹)

    public TMP_InputField inputField;
    public Button sendButton;            // 전송 버튼 (없으면 Inspector에서 비워도 됨)

    void Awake()
    {
        // 1. 싱글톤 초기화 (빠져있던 부분)
        Instance = this;
    }

    void Start()
    {
        // 2. 이벤트 리스너 연결 (빠져있던 부분)
        if (sendButton != null)
            sendButton.onClick.AddListener(SendMessage);

        if (inputField != null)
            inputField.onSubmit.AddListener((val) => SendMessage());
    }

    // [UI -> Network] 메시지 전송 시도
    public void SendMessage()
    {
        if (string.IsNullOrWhiteSpace(inputField.text)) return;

        // 네트워크 매니저가 없으면 중단 (안전장치)
        if (NetworkManager.Instance == null)
        {
            Debug.LogError("NetworkManager Instance is null!");
            return;
        }

        // 서버로 전송
        NetworkManager.Instance.SendChat(inputField.text);

        // UI 초기화
        inputField.text = "";
        inputField.ActivateInputField(); // 엔터 치고 나서도 바로 다시 입력 가능하게 포커스 유지
    }

    // [Network -> UI] 일반 채팅 메시지 수신
    public void AddChatMessage(string msg)
    {
        CreateChatObject(msg);
    }

    // [Network -> UI] 시스템 메시지 수신 (색상 포함)
    public void AddSystemMessage(string msg, Color color)
    {
        string colorHex = ColorUtility.ToHtmlStringRGB(color);
        string formattedMsg = $"<color=#{colorHex}>{msg}</color>";

        CreateChatObject(formattedMsg);
    }

    // 내부적으로 텍스트 생성하는 함수 (중복 제거)
    private void CreateChatObject(string msg)
    {
        // 프리팹 생성
        GameObject newText = Instantiate(textPrefab, contentTransform);

        // 텍스트 컴포넌트 찾아서 값 설정
        TextMeshProUGUI textComp = newText.GetComponent<TextMeshProUGUI>();
        if (textComp != null)
        {
            textComp.text = msg;
        }

        // 자동 스크롤
        StartCoroutine(AutoScrollToBottom());
    }

    IEnumerator AutoScrollToBottom()
    {
        // 프레임 엔드까지 기다려야 UI 사이즈 계산이 끝남
        yield return new WaitForEndOfFrame();

        // 스크롤을 맨 아래로 내림
        scrollRect.verticalNormalizedPosition = 0f;
    }
}