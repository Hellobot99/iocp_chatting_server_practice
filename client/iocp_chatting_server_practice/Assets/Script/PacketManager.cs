using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
// using UnityEditor; // 빌드 시 에러 날 수 있으므로 필요 없으면 제거 추천
using UnityEngine;
// using UnityEngine.LightTransport; // 필요 없으면 제거

public class PacketManager
{
    // 싱글톤 패턴
    private static PacketManager _instance;
    public static PacketManager Instance
    {
        get
        {
            if (_instance == null) _instance = new PacketManager();
            return _instance;
        }
    }

    // 패킷 수신 시 호출되는 함수 (NetworkManager가 호출)
    public void ProcessPacket(PacketId id, byte[] bodyData)
    {
        switch (id)
        {
            // --------------------------------------------------------
            // [1] 계정 관련 (추가됨)
            // --------------------------------------------------------
            case PacketId.REGISTER_RES:
                HandlePacket<PacketRegisterRes>(bodyData, PacketHandler.HandleRegisterRes);
                break;

            case PacketId.LOGIN_RES:
                HandlePacket<PacketLoginRes>(bodyData, PacketHandler.HandleLoginRes);
                break;

            // --------------------------------------------------------
            // [2] 로비 관련 (추가됨)
            // --------------------------------------------------------
            case PacketId.CREATE_ROOM_RES:
                HandlePacket<PacketCreateRoomRes>(bodyData, PacketHandler.HandleCreateRoomRes);
                break;

            case PacketId.ROOM_LIST_RES:
                // 방 목록은 가변 길이 데이터(배열)이므로 구조체 변환 없이 Raw Data를 바로 넘깁니다.
                PacketHandler.HandleRoomList(bodyData);
                break;

            // --------------------------------------------------------
            // [3] 인게임 플레이
            // --------------------------------------------------------
            case PacketId.CHAT:
                HandlePacket<PacketChat>(bodyData, PacketHandler.HandleChatPacket);
                break;

            case PacketId.MOVE:
                HandlePacket<PacketMove>(bodyData, PacketHandler.HandleMovePacket);
                break;

            case PacketId.SNAPSHOT:
                // 스냅샷도 가변 길이(플레이어 수에 따라 다름)이므로 Raw Data 전달
                PacketHandler.HandleSnapshot(bodyData);
                break;

            case PacketId.LEAVE_ROOM:
                HandlePacket<PacketLeaveRoom>(bodyData, PacketHandler.HandleLeavePacket);
                break;
        }
    }

    // 제네릭 헬퍼 함수: 바이트 배열 -> 구조체 변환 후 핸들러 호출
    void HandlePacket<T>(byte[] data, Action<T> handler) where T : struct
    {
        T pkt = ByteArrayToStructure<T>(data);
        handler.Invoke(pkt);
    }

    // 바이트 배열 -> 구조체 변환
    public static T ByteArrayToStructure<T>(byte[] bytearray) where T : struct
    {
        int len = Marshal.SizeOf(typeof(T));
        if (len > bytearray.Length) return default(T); // 안전장치

        IntPtr ptr = Marshal.AllocHGlobal(len);
        try
        {
            Marshal.Copy(bytearray, 0, ptr, len);
            return (T)Marshal.PtrToStructure(ptr, typeof(T));
        }
        finally
        {
            // [중요] 메모리 누수 방지를 위해 finally 블록에서 해제 추천
            Marshal.FreeHGlobal(ptr);
        }
    }
}