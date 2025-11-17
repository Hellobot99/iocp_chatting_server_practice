#include "Command.h"
#include "GameRoom.h"
#include "Persistence.h"
#include "PlayerState.h"

ICommand::~ICommand() {}

void MoveCommand::Execute(RoomManager& roomManager, Persistence& persistence) {

}

void ChatCommand::Execute(RoomManager& roomManager, Persistence& persistence) {

}