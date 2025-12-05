using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.LightTransport;

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
            case PacketId.CHAT:
                HandlePacket<PacketChat>(bodyData, PacketHandler.HandleChatPacket);
                break;

            case PacketId.MOVE:
                HandlePacket<PacketMove>(bodyData, PacketHandler.HandleMovePacket);
                break;

            case PacketId.SNAPSHOT:
                PacketHandler.HandleSnapshot(bodyData);
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
        Marshal.Copy(bytearray, 0, ptr, len);
        T str = (T)Marshal.PtrToStructure(ptr, typeof(T));
        Marshal.FreeHGlobal(ptr);
        return str;
    }
}