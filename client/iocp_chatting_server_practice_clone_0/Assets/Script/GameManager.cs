using UnityEngine;
using System.Collections.Generic;

public class GameManager : MonoBehaviour
{
    public static GameManager Instance;

    [Header("Resources")]
    public GameObject playerPrefab; // PlayerController가 붙은 프리팹

    // 접속한 플레이어 목록
    private Dictionary<uint, PlayerController> _players = new Dictionary<uint, PlayerController>();

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

    public void SpawnPlayer(uint playerId, bool isLocal, Vector3 pos)
    {
        if (_players.ContainsKey(playerId)) return;
        if (playerPrefab == null)
        {
            Debug.LogError("PlayerPrefab is missing!");
            return;
        }

        GameObject go = Instantiate(playerPrefab, pos, Quaternion.identity);
        PlayerController pc = go.GetComponent<PlayerController>();

        if (pc != null)
        {
            pc.playerId = playerId;
            pc.isLocalPlayer = isLocal;

            if (isLocal)
            {
                NetworkManager.Instance.MyPlayerId = playerId;
                // 카메라 추적 로직 등을 여기에 추가
            }
        }

        _players.Add(playerId, pc);
        Debug.Log($"Spawned Player: {playerId} (Local: {isLocal})");
    }

    public void DespawnPlayer(uint playerId)
    {
        if (_players.ContainsKey(playerId))
        {
            Destroy(_players[playerId].gameObject);
            _players.Remove(playerId);
        }
    }

    // ID로 플레이어 찾기 (이동 패킷 처리용)
    public PlayerController GetPlayer(uint playerId)
    {
        if (_players.TryGetValue(playerId, out PlayerController pc))
            return pc;
        return null;
    }
}