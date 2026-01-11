using TMPro;
using UnityEngine;
using UnityEngine.SceneManagement;
using UnityEngine.UI;

public class LobbyUI : MonoBehaviour
{
    public static LobbyUI Instance; // 싱글톤

    [Header("Panels")]
    public GameObject loginPanel;    // [1단계] 로그인/회원가입 화면
    public GameObject lobbyPanel;    // [2단계] 방 목록/방 만들기 화면
    public GameObject gamePanel;     // [3단계] 인게임(채팅) 화면

    [Header("Login UI Elements")]
    public TMP_InputField nameInput;
    public TMP_InputField passwordInput;
    public Button LoginButton;
    public Button registerButton;

    [Header("Lobby UI Elements (Room List)")]
    public Transform contentParent;  // ScrollView의 Content
    public GameObject roomItemPrefab;
    public Button refreshButton;     // [수정] 새로고침 버튼
    public Button openCreatePanelButton;
    public Button LogoutButton;

    [Header("Create Room UI")]
    public GameObject createRoomPanel; // 로비 패널 위에 뜨는 팝업
    public TMP_InputField roomTitleInput;
    public Button confirmCreateButton;
    public Button cancelCreateButton;

    void Awake()
    {
        Instance = this;

        // 시작할 때: 로그인 패널만 켜고 나머진 끈다
        loginPanel.SetActive(true);
        lobbyPanel.SetActive(false);
        gamePanel.SetActive(false);
        createRoomPanel.SetActive(false);
    }

    void Start()
    {
        // ------------------------------------------------
        // [1] 로그인 화면 이벤트
        // ------------------------------------------------
        if (LoginButton != null)
            LoginButton.onClick.AddListener(OnLoginButtonClicked);

        if (registerButton != null)
            registerButton.onClick.AddListener(OnRegisterButtonClicked);

        // ------------------------------------------------
        // [2] 로비(방 목록) 화면 이벤트
        // ------------------------------------------------
        // [누락된 부분 수정] 새로고침 버튼 연결
        if (refreshButton != null)
            refreshButton.onClick.AddListener(OnRefreshClicked);

        if (LogoutButton != null)
            LogoutButton.onClick.AddListener(OnLogoutClicked);

        openCreatePanelButton.onClick.AddListener(() => {
            createRoomPanel.SetActive(true);
            roomTitleInput.text = "";
        });

        // ------------------------------------------------
        // [3] 방 만들기 팝업 이벤트
        // ------------------------------------------------
        cancelCreateButton.onClick.AddListener(() => {
            createRoomPanel.SetActive(false);
        });

        confirmCreateButton.onClick.AddListener(() => {
            string title = roomTitleInput.text;
            if (string.IsNullOrWhiteSpace(title)) title = "New Room";

            NetworkManager.Instance.SendCreateRoom(title);
            createRoomPanel.SetActive(false);
        });
    }

    // -----------------------------------------------------------
    // 버튼 클릭 핸들러들
    // -----------------------------------------------------------

    void OnLoginButtonClicked()
    {
        NetworkManager.Instance.ConnectToServer();

        string myName = nameInput.text;
        string myPassword = passwordInput.text;

        if (string.IsNullOrWhiteSpace(myName) || string.IsNullOrWhiteSpace(myPassword))
        {
            Debug.LogError("아이디와 비밀번호를 입력해주세요.");
            return;
        }

        NetworkManager.Instance.SendLogin(myName, myPassword);
        // 주의: 여기서 바로 패널을 넘기지 않음 (HandleLoginRes에서 처리)
    }

    void OnRegisterButtonClicked()
    {
        NetworkManager.Instance.ConnectToServer();

        string myName = nameInput.text;
        string myPassword = passwordInput.text;

        if (string.IsNullOrWhiteSpace(myName) || string.IsNullOrWhiteSpace(myPassword))
        {
            Debug.LogError("가입할 아이디와 비밀번호를 입력하세요.");
            return;
        }

        NetworkManager.Instance.SendRegister(myName, myPassword);
        Debug.Log("회원가입 요청 전송함.");
    }

    void OnRefreshClicked()
    {
        // 방 목록 요청
        NetworkManager.Instance.SendPacket(PacketId.ROOM_LIST_REQ, new byte[0]);
    }

    void OnLogoutClicked()
    {
        // 방 목록 요청
        NetworkManager.Instance.SendLogout();

        loginPanel.SetActive(true);
        lobbyPanel.SetActive(false);
        gamePanel.SetActive(false);
    }

    // -----------------------------------------------------------
    // 외부(PacketHandler)에서 호출할 UI 제어 함수들
    // -----------------------------------------------------------

    // 1. 로그인 성공 시 -> 로비 패널로 이동
    public void OnLoginSuccess()
    {
        loginPanel.SetActive(false);
        lobbyPanel.SetActive(true);
        gamePanel.SetActive(false);

        // 로비 진입하자마자 목록 갱신 요청
        OnRefreshClicked();
    }

    // 2. 방 입장 성공 시 -> 채팅 패널로 이동
    public void OnEnterRoomSuccess()
    {
        loginPanel.SetActive(false);
        lobbyPanel.SetActive(false);
        gamePanel.SetActive(true);
    }

    // -----------------------------------------------------------
    // 방 목록 관리 함수들
    // -----------------------------------------------------------

    public void ClearRoomList()
    {
        foreach (Transform child in contentParent)
            Destroy(child.gameObject);
    }

    public void AddRoomItem(int id, string title, int count)
    {
        // 1. 프리팹 확인
        if (roomItemPrefab == null)
        {
            Debug.LogError("LobbyUI에 RoomItemPrefab이 연결되지 않았습니다!");
            return;
        }

        // 2. Content Parent 확인
        if (contentParent == null)
        {
            Debug.LogError("LobbyUI에 ContentParent가 연결되지 않았습니다!");
            return;
        }

        GameObject go = Instantiate(roomItemPrefab, contentParent);
        RoomItem item = go.GetComponent<RoomItem>();

        // 3. 스크립트 부착 여부 확인
        if (item == null)
        {
            Debug.LogError("프리팹(RoomItem)에 RoomItem 스크립트가 붙어있지 않습니다!");
            // 임시 방편으로 붙여주거나 리턴
            // item = go.AddComponent<RoomItem>(); 
            return;
        }

        item.Setup(id, title, count);
    }
}