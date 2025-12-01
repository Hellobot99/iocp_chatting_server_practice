using System.Runtime.InteropServices;

// 패킷 ID (서버와 동일해야 함)
public enum PacketId : ushort
{
    LOGIN = 1,
    CHAT = 2,
    MOVE = 3,
    SNAPSHOT = 4
}

// 헤더
[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct GameHeader
{
    public ushort packetSize;
    public ushort packetId;
}

// ---------------- 패킷 구조체들 ----------------

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct PacketChat
{
    public uint playerId;
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 256)]
    public byte[] msg;
}

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct PacketMove
{
    public float vx;
    public float vy;
}

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct PacketLogin
{
    public int roomId;

    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)]
    public byte[] name;
}