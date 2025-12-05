using UnityEngine;
using System.Collections;
using System;

public class PlayerController : MonoBehaviour
{
    [Header("Network Settings")]
    public uint playerId;
    public bool isLocalPlayer;

    [Header("Movement Settings")]
    public float moveSpeed = 5.0f;

    private Vector3 serverPosition;
    private Vector2 _lastSentVelocity = Vector2.zero;

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

        // 1. 현재 프레임의 목표 속도 계산
        Vector2 currentVelocity = new Vector2(h, v).normalized * moveSpeed;

        // 2. [핵심] "이번 속도"가 "마지막으로 보낸 속도"와 다를 때만 전송
        // (움직이기 시작할 때, 방향 바꿀 때, 그리고 ★멈출 때★ 전송됨)
        if (currentVelocity != _lastSentVelocity)
        {
            if (NetworkManager.Instance != null)
            {
                PacketMove movePkt = new PacketMove
                {
                    vx = currentVelocity.x,
                    vy = currentVelocity.y
                };

                NetworkManager.Instance.SendPacket(PacketId.MOVE, movePkt);
                _lastSentVelocity = currentVelocity; // 보낸 속도 갱신

                // 디버그 로그 (확인용)
                // Debug.Log($"Packet Sent: {currentVelocity}");
            }
        }

        // 로컬 이동 (예측 이동)
        // 입력이 없으면(0,0) Translate도 0이므로 움직이지 않게 됨
        transform.Translate(currentVelocity * Time.deltaTime);
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