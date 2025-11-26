#include "PlayerState.h"

void PlayerState::ApplyMovement(float fixedDeltaTime)
{
    position.x += velocity.x * fixedDeltaTime;
    position.y += velocity.y * fixedDeltaTime;

    // 로직 추가 가능
}