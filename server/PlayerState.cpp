#include "PlayerState.h"

void PlayerState::ApplyMovement(float fixedDeltaTime)
{
    position.x += velocity.x * fixedDeltaTime;
    position.y += velocity.y * fixedDeltaTime;
}