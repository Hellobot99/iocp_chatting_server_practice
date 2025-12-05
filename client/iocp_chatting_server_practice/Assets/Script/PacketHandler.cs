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

                // 4. 위치 업데이트
                GameManager.Instance.UpdatePlayerPosition(playerId, x, y);
            }
        });
    }
}