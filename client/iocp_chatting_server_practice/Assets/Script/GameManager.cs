using System.Collections.Generic;
using UnityEngine;
using static UnityEditor.Experimental.GraphView.GraphView;

public class GameManager : MonoBehaviour
{
    public static GameManager Instance;

    [Header("Resources")]
    public GameObject playerPrefab; // PlayerController가 붙은 프리팹

    // 접속한 플레이어 목록
    private Dictionary<uint, PlayerController> players = new Dictionary<uint, PlayerController>();
    public uint MyPlayerId;

    void Awake()
    {
        Instance = this;
    }

    void Start()
    {
        // 테스트용: 게임 시작 시 내 캐릭터(999) 생성
        // 실제로는 로그인 패킷 응답(S_LOGIN) 받았을 때 호출해야 함
        //SpawnPlayer(999, true, Vector3.zero);
    }

    void SpawnPlayer(uint playerId, float x, float y)
    {
        GameObject go = Instantiate(playerPrefab, new Vector3(x, y, 0), Quaternion.identity);
        PlayerController pc = go.GetComponent<PlayerController>();

        pc.playerId = playerId;
        pc.isLocalPlayer = (playerId == NetworkManager.Instance.MyPlayerId); // 내 ID와 비교

        players.Add(playerId, pc);
    }

    public void UpdatePlayerPosition(uint playerId, float x, float y)
    {
        // 1. 이미 존재하는 플레이어인가?
        if (players.ContainsKey(playerId))
        {
            PlayerController pc = players[playerId];

            // 내 캐릭터(Local Player)는 서버 위치를 무시하고 예측 이동을 하거나,
            // 오차가 너무 클 때만 살짝 보정해줍니다. (여기선 간단히 리모트만 처리)
            if (!pc.isLocalPlayer)
            {
                pc.SetServerPosition(x, y);
            }
        }
        else
        {
            // 2. 없는 플레이어라면 새로 생성 (Spawn)
            SpawnPlayer(playerId, x, y);
        }
    }

    public void DespawnPlayer(uint playerId)
    {
        if (players.ContainsKey(playerId))
        {
            Destroy(players[playerId].gameObject);
            players.Remove(playerId);
        }
    }

    // ID로 플레이어 찾기 (이동 패킷 처리용)
    public PlayerController GetPlayer(uint playerId)
    {
        if (players.TryGetValue(playerId, out PlayerController pc))
            return pc;
        return null;
    }
}