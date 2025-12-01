using UnityEngine;
using System.Text;
using PimDeWitte.UnityMainThreadDispatcher;

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
}