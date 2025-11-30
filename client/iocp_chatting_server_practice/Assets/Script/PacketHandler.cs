using UnityEngine;
using System.Text;
using PimDeWitte.UnityMainThreadDispatcher;

public class PacketHandler
{
    public static void HandleChatPacket(PacketChat pkt)
    {
        // 유니티 메인 스레드에서 UI를 건드려야 하므로 Dispatcher 사용
        UnityMainThreadDispatcher.Instance().Enqueue(() =>
        {
            // 1. 바이트 배열을 문자열로 변환
            string msg = Encoding.UTF8.GetString(pkt.msg).TrimEnd('\0');

            // 2. ChatUI 싱글톤에 접근해서 화면에 출력 요청
            if (ChatUI.Instance != null)
            {
                ChatUI.Instance.AddChatMessage($"User({pkt.playerId}): {msg}");
            }
        });
    }

    public static void HandleMovePacket(PacketMove pkt)
    {
        // 서버에서 이동 확인 패킷이 온다면 처리 (현재는 로직이 없어서 로그만)
        Debug.Log($"Recv Move Packet: {pkt.vx}, {pkt.vy}");
    }
}