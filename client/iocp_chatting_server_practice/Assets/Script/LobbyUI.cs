using UnityEngine;
using UnityEngine.UI;
using TMPro; // TextMeshPro 사용 시

public class LobbyUI : MonoBehaviour
{
    [Header("Panels")]
    public GameObject lobbyPanel;
    public GameObject chatPanel;

    [Header("UI Elements")]
    public Button joinButton;
    public TMP_InputField nameInput;
    public TMP_InputField roomInput;

    void Start()
    {
        // 버튼에 클릭 이벤트 연결
        joinButton.onClick.AddListener(OnJoinButtonClicked);
    }

    void OnJoinButtonClicked()
    {
        NetworkManager.Instance.ConnectToServer();

        string myName = nameInput.text;
        if (string.IsNullOrWhiteSpace(myName)) myName = "Guest";

        // [추가] 방 번호 파싱 (입력 안 하면 0번방)
        int roomNum = 0;
        int.TryParse(roomInput.text, out roomNum);

        // SendLogin에 방 번호도 같이 전달
        NetworkManager.Instance.SendLogin(roomNum, myName);

        lobbyPanel.SetActive(false);
        chatPanel.SetActive(true);
    }
}