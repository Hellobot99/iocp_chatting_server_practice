using System;
using System.Runtime.InteropServices;
using System.Text;

// [패킷 ID] 서버의 PacketId enum과 순서/번호가 완전히 같아야 합니다.
public enum PacketId : ushort
{
    // [로그인/계정]
    REGISTER_REQ = 1,
    REGISTER_RES = 2,
    LOGIN_REQ = 3,
    LOGIN_RES = 4,

    // [인게임/로비]
    ENTER_ROOM = 5,
    LEAVE_ROOM = 6,

    // [플레이]
    CHAT = 7,
    MOVE = 8,
    SNAPSHOT = 9,

    ROOM_LIST_REQ = 10,
    ROOM_LIST_RES = 11,

    CREATE_ROOM_REQ = 12,
    CREATE_ROOM_RES = 13,
}

// [헤더]
[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct GameHeader
{
    public ushort packetSize;
    public ushort packetId;
}

// --------------------------------------------------
// [1] 계정 관련 패킷
// --------------------------------------------------

// 회원가입 요청
[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct PacketRegisterReq
{
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 50)]
    public byte[] username;

    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 50)]
    public byte[] password;
}

// 회원가입 응답
[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct PacketRegisterRes
{
    [MarshalAs(UnmanagedType.I1)] // C++ bool (1byte) 대응
    public bool success;
}

// 로그인 요청
[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct PacketLoginReq
{
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 50)]
    public byte[] username;

    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 50)]
    public byte[] password;
}

// 로그인 응답
[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct PacketLoginRes
{
    [MarshalAs(UnmanagedType.I1)]
    public bool success;
    public uint playerId;
}

// --------------------------------------------------
// [2] 룸 입장/퇴장
// --------------------------------------------------

// 방 입장 요청
[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct PacketEnterRoom
{
    public int roomId;
}

// 방 퇴장 알림
[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct PacketLeaveRoom
{
    public uint playerId;
}

// --------------------------------------------------
// [3] 인게임 플레이
// --------------------------------------------------

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct PacketMove
{
    public float vx;
    public float vy;
}

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct PacketChat
{
    public uint playerId;

    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 256)]
    public byte[] msg;
}

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct RoomInfo
{
    public int roomId;
    public int userCount;
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)]
    public byte[] title;
}

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct PacketRoomListRes
{
    public int count;
}

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct PacketCreateRoomReq
{
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)]
    public byte[] title;
}

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct PacketCreateRoomRes
{
    [MarshalAs(UnmanagedType.I1)]
    public bool success;
    public int roomId;
}