using UnityEngine;
using System.Collections;

public class PlayerController : MonoBehaviour
{
    [Header("Network Settings")]
    public uint playerId;
    public bool isLocalPlayer;

    [Header("Movement Settings")]
    public float moveSpeed = 5.0f;

    private Vector3 serverPosition;

    void Start()
    {
        // NetworkManager는 싱글톤으로 접근하므로 FindObjectOfType 불필요
        // 초기 위치 동기화
        serverPosition = transform.position;
    }

    void Update()
    {
        if (isLocalPlayer)
        {
            HandleInput();
        }
        else
        {
            // 리모트 플레이어: 서버 위치로 보간 이동
            transform.position = Vector3.Lerp(transform.position, serverPosition, Time.deltaTime * 10f);
        }
    }

    void HandleInput()
    {
        float h = Input.GetAxisRaw("Horizontal");
        float v = Input.GetAxisRaw("Vertical");

        // 입력이 있거나, 이동 중일 때만 전송 (최적화 가능)
        // 여기서는 간단하게 키 입력이 있을 때만 체크
        if (h == 0 && v == 0) return;

        if (NetworkManager.Instance != null)
        {
            // 이동 속도 벡터 계산 (정규화 포함)
            Vector2 velocity = new Vector2(h, v).normalized * moveSpeed;

            // [변경] 구조체 생성 후 NetworkManager를 통해 전송
            PacketMove movePkt = new PacketMove
            {
                vx = velocity.x,
                vy = velocity.y
            };

            NetworkManager.Instance.SendPacket(PacketId.MOVE, movePkt);

            // 로컬에서의 즉시 반응성을 위해 클라이언트 예측 이동 (Client-side Prediction)
            // 서버가 위치를 강제하는 방식이라면 이 부분은 주석 처리하거나, 
            // 서버 응답이 늦을 때를 대비해 살짝 섞어 쓸 수 있습니다.
            transform.Translate(velocity * Time.deltaTime);
        }
    }

    // [리모트/로컬 공통] 서버에서 받은 위치 데이터 적용 (PacketHandler에서 호출 예정)
    public void SetServerPosition(float x, float y)
    {
        serverPosition = new Vector3(x, y, 0);

        // 만약 거리가 너무 멀면 순간이동 (Teleport) - 위치 오차 보정
        if (Vector3.Distance(transform.position, serverPosition) > 2.0f)
        {
            transform.position = serverPosition;
        }
    }
}