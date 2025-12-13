using PimDeWitte.UnityMainThreadDispatcher;
using System;
using System.Text;
using UnityEngine;

public class PacketHandler
{
    public static void HandleChatPacket(PacketChat pkt)
    {
        UnityMainThreadDispatcher.Instance().Enqueue(() =>
        {
            // 1. 바이트 배열 -> 문자열 변환
            string msg = Encoding.UTF8.GetString(pkt.msg).TrimEnd('\0');

            if (ChatUI.Instance != null)
            {
                ChatUI.Instance.AddChatMessage(msg);
            }
        });
    }

    public static void HandleMovePacket(PacketMove pkt)
    {
        // 서버에서 이동 확인 패킷이 온다면 처리 (현재는 로직이 없어서 로그만)
        Debug.Log($"Recv Move Packet: {pkt.vx}, {pkt.vy}");
    }

    public static void HandleSnapshot(byte[] data)
    {
        // data에는 이미 헤더가 없으므로 0번지부터가 바로 내용(Count)입니다.
        int offset = 0;

        // 1. 데이터 검증 (최소한 Count 4바이트는 있어야 함)
        if (data.Length < 4) return;

        // 2. 플레이어 수 읽기
        int count = BitConverter.ToInt32(data, offset);
        offset += 4;

        UnityMainThreadDispatcher.Instance().Enqueue(() =>
        {
            if (GameManager.Instance == null) return;

            for (int i = 0; i < count; i++)
            {
                // 남은 데이터 길이 체크 (ID(4) + X(4) + Y(4) = 12바이트)
                if (offset + 12 > data.Length) break;

                // 3. 순서대로 파싱 (서버 GameRoom.cpp에서 넣은 순서: ID -> X -> Y)
                uint playerId = BitConverter.ToUInt32(data, offset);
                offset += 4;
                
                float x = BitConverter.ToSingle(data, offset);
                offset += 4;
                
                float y = BitConverter.ToSingle(data, offset);
                offset += 4;

                if (LobbyUI.Instance != null && !LobbyUI.Instance.gamePanel.activeSelf)
                {
                    LobbyUI.Instance.OnEnterRoomSuccess();
                }

                if (GameManager.Instance == null) return;

                // 4. 위치 업데이트
                GameManager.Instance.UpdatePlayerPosition(playerId, x, y);
            }
        });


    }
    public static void HandleLeavePacket(PacketLeaveRoom pkt)
    {
        UnityMainThreadDispatcher.Instance().Enqueue(() =>
        {
            if (GameManager.Instance != null)
            {
                GameManager.Instance.DespawnPlayer(pkt.playerId);
            }
        });
    }

    // [추가] 회원가입 결과 처리
    public static void HandleRegisterRes(PacketRegisterRes pkt)
    {
        UnityMainThreadDispatcher.Instance().Enqueue(() =>
        {
            if (pkt.success)
            {
                Debug.Log("[Client] 회원가입 성공! 로그인을 진행하세요.");
                // TODO: UI에 "가입 성공" 팝업 띄우기
            }
            else
            {
                Debug.LogError("[Client] 회원가입 실패 (중복된 아이디 등)");
                // TODO: UI에 "가입 실패" 알림
            }
        });
    }

    // [추가] 로그인 결과 처리
    public static void HandleLoginRes(PacketLoginRes pkt)
    {
        UnityMainThreadDispatcher.Instance().Enqueue(() =>
        {
            if (pkt.success)
            {
                Debug.Log($"[Client] 로그인 성공! (PlayerID: {pkt.playerId})");

                if (GameManager.Instance != null)
                {
                    GameManager.Instance.MyPlayerId = pkt.playerId;
                }

                // [중요] UI 전환: 로그인 화면 -> 로비(방 목록) 화면
                if (LobbyUI.Instance != null)
                {
                    LobbyUI.Instance.OnLoginSuccess();
                }
            }
            else
            {
                Debug.LogError("[Client] 로그인 실패 (아이디/비번 확인)");
                // 실패 팝업 처리 등...
            }
        });
    }

    public static void HandleRoomList(byte[] data)
    {
        // 1. 방 개수 읽기
        int offset = 0;
        int count = BitConverter.ToInt32(data, offset);
        offset += 4; // sizeof(int)

        UnityMainThreadDispatcher.Instance().Enqueue(() =>
        {
            if (LobbyUI.Instance != null)
            {
                // UI 초기화 (기존 목록 삭제)
                LobbyUI.Instance.ClearRoomList();

                // 2. 방 개수만큼 반복 파싱
                for (int i = 0; i < count; i++)
                {
                    // RoomInfo 구조체 크기만큼 읽기
                    // int(4) + int(4) + char(32) = 40바이트

                    int rId = BitConverter.ToInt32(data, offset);
                    offset += 4;

                    int uCount = BitConverter.ToInt32(data, offset);
                    offset += 4;

                    // 문자열 파싱 (32바이트 고정)
                    string title = Encoding.UTF8.GetString(data, offset, 32).TrimEnd('\0');
                    offset += 32;

                    // UI에 추가
                    LobbyUI.Instance.AddRoomItem(rId, title, uCount);
                }
            }
        });
    }

    public static void HandleCreateRoomRes(PacketCreateRoomRes pkt)
    {
        UnityMainThreadDispatcher.Instance().Enqueue(() =>
        {
            if (pkt.success)
            {
                Debug.Log($"[Client] 방 생성 성공! (Room ID: {pkt.roomId})");

                // 생성 성공 시 바로 입장 요청
                NetworkManager.Instance.SendEnterRoom(pkt.roomId);
            }
            else
            {
                Debug.LogError("[Client] 방 생성 실패");
            }
        });
    }
}