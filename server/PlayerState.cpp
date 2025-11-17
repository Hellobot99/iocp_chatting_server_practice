#include "PlayerState.h"

void PlayerState::ApplyMovement(float fixedDeltaTime)
{
    position.x += velocity.x * fixedDeltaTime;
    position.y += velocity.y * fixedDeltaTime;

    //로직을 여기에 추가 가능
}