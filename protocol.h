// protocol.h
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "common.h"

// messages from server -> client (payload is ASCII text)
#define MSG_WELCOME "WELCOME"        // WELCOME <player_id>
#define MSG_WAITING "WAITING"        // WAITING <connected>/<needed>
#define MSG_GAME_START "GAME_START"
#define MSG_DEAL "DEAL"              // DEAL <card1> <card2>
#define MSG_YOUR_TURN "YOUR_TURN"
#define MSG_REQUEST_ACTION "REQUEST_ACTION"
#define MSG_CARD "CARD"              // CARD <card>
#define MSG_BUSTED "BUSTED"
#define MSG_RESULT "RESULT"          // RESULT <WIN|LOSE|PUSH> <player_total> <dealer_total>
#define MSG_BROADCAST "BROADCAST"
#define MSG_ERROR "ERROR"
#define MSG_GOODBYE "GOODBYE"

// messages from client -> server
#define CMD_JOIN "JOIN"              // JOIN <name>
#define CMD_ACTION "ACTION"          // ACTION HIT / ACTION STAND
#define CMD_QUIT "QUIT"
#define CMD_CHAT "CHAT"

#endif // PROTOCOL_H
