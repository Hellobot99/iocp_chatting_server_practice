using UnityEngine;
using TMPro;
using UnityEngine.UI;

public class RoomItem : MonoBehaviour
{
    public TextMeshProUGUI titleText;
    public TextMeshProUGUI countText;
    public Button joinButton;
    private int roomId;

    public void Setup(int id, string title, int count)
    {
        roomId = id;
        titleText.text = title;
        countText.text = $"({count}명)";

        if (joinButton != null)
        {
            joinButton.onClick.RemoveAllListeners();
            joinButton.onClick.AddListener(() => {
                NetworkManager.Instance.SendEnterRoom(roomId);
            });
        }
        else
        {
            Debug.LogError("RoomItem 프리팹에 Button이 연결되지 않았습니다!");
        }
    }
}