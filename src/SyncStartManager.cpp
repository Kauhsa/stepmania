// idk what I need exactly, this should be ok
#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <algorithm>
#include <sstream>
#include <iomanip>

#include "global.h"
#include "SyncStartManager.h"
#include "ScreenManager.h"
#include "SongManager.h"
#include "ScreenSelectMusic.h"
#include "PlayerNumber.h"
#include "ProfileManager.h"
#include "RageLog.h"

SyncStartManager *SYNCMAN;

#define BUFSIZE 1024
#define PORT 53000

// opcodes

#define START 0x00
#define SONG 0x01
#define SCORE 0x02

bool operator<(const ScoreKey& l, const ScoreKey& r) {
	return l.machineAddress.s_addr < r.machineAddress.s_addr || l.playerNumber < r.playerNumber || l.playerName < r.playerName;
}

bool sortScorePairs(const std::pair<ScoreKey, ScoreValue>& l, const std::pair<ScoreKey, ScoreValue>& r) {
	return r.second.score < l.second.score;
}

std::vector<std::string> split(const std::string& str, const std::string& delim)
{
    std::vector<std::string> tokens;
    size_t prev = 0, pos = 0;
    do
    {
        pos = str.find(delim, prev);
        if (pos == std::string::npos) pos = str.length();
        std::string token = str.substr(prev, pos-prev);
        if (!token.empty()) tokens.push_back(token);
        prev = pos + delim.length();
    }
    while (pos < str.length() && prev < str.length());
    return tokens;
}

SyncStartManager::SyncStartManager()
{
	// Register with Lua.
	{
		Lua *L = LUA->Get();
		lua_pushstring( L, "SYNCMAN" );
		this->PushSelf( L );
		lua_settable(L, LUA_GLOBALSINDEX);
		LUA->Release( L );
	}	

	this->socketfd = -1;
	this->enabled = false;
}

SyncStartManager::~SyncStartManager()
{
	this->disable();
}

bool SyncStartManager::isEnabled()
{
	return this->enabled;
}

void SyncStartManager::enable()
{
	// initialize
	this->socketfd = socket(AF_INET, SOCK_DGRAM, 0);

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(PORT);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	// we need to be able to broadcast through this socket
	int broadcast = 1;
	if (setsockopt(this->socketfd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) == -1) {
		return;
	}

	if (bind(this->socketfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		return;
	}

	this->enabled = true;
}

void SyncStartManager::broadcast(char code, std::string msg) {
	if (!this->enabled) {
		return;
	}

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(PORT);
	addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	
	// first byte is code, rest is the message
	char buffer[BUFSIZE];
	buffer[0] = code;
	std::size_t length = msg.copy(buffer + 1, BUFSIZE - 1, 0);

	LOG->Info("BROADCASTING: code %d, msg: '%s'", code, msg.c_str());

	if (sendto(this->socketfd, &buffer, length + 1, 0, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
		return;
	}
}

void SyncStartManager::broadcastStarting()
{
	this->broadcast(START, "");
}

void SyncStartManager::broadcastSongPath(std::string songPath) {
	this->broadcast(SONG, songPath);
}

void SyncStartManager::broadcastScoreChange(int noteRow, const PlayerStageStats& pPlayerStageStats) {
	stringstream msg;

	msg << PROFILEMAN->GetPlayerName(pPlayerStageStats.m_player_number) << '|';
	msg << noteRow << '|';
	msg << pPlayerStageStats.GetPercentDancePoints() << '|';
	msg << pPlayerStageStats.GetCurrentLife() << '|';
	msg << (pPlayerStageStats.m_bFailed ? '1' : '0') << '|';

	for (int i = 0; i < NUM_TapNoteScore; ++i) {
		msg << pPlayerStageStats.m_iTapNoteScores[i] << '|';
	}

	for (int i = 0; i < NUM_HoldNoteScore; ++i) {
		msg << pPlayerStageStats.m_iHoldNoteScores[i] << '|';
	}

	this->broadcast(SCORE, msg.str()); 
}

void SyncStartManager::receiveScoreChange(struct in_addr in_addr, const std::string& msg) {
	ScoreKey scoreKey = {
		.machineAddress = in_addr
	};

	ScoreValue scoreValue;

	try {
		auto items = split(msg, "|");
		auto iter = items.begin();
		scoreKey.playerName = *iter++;
		iter++; // noterow, ignore for now
		scoreValue.score = std::stof(*iter++);
		scoreValue.life = std::stof(*iter++);
		scoreValue.failed = *iter++ == "1" ? true : false;

		for (int i = 0; i < NUM_TapNoteScore; ++i) {
			scoreValue.tapNoteScores[i] = std::stoi(*iter++);
		}

		for (int i = 0; i < NUM_HoldNoteScore; ++i) {
			scoreValue.holdNoteScores[i] = std::stoi(*iter++);
		}

		playerScores[scoreKey] = scoreValue;

		MESSAGEMAN->Broadcast("SyncStartPlayerScoresChanged");
	} catch (std::exception& e) {
		LOG->Warn("Could not parse score change '%s'", msg);
		// do nothing, just don't crash!
	}
}

void SyncStartManager::disable()
{
	if (this->socketfd > 0)
	{
		shutdown(this->socketfd, SHUT_RDWR);
		close(this->socketfd);
	}

	this->enabled = false;
}

int SyncStartManager::getNextMessage(char* buffer, sockaddr_in* remaddr, size_t bufferSize) {
	socklen_t addrlen = sizeof remaddr;
	ssize_t received;
	return recvfrom(this->socketfd, buffer, bufferSize, MSG_DONTWAIT, (struct sockaddr *) remaddr, &addrlen);
}

void SyncStartManager::Update() {
	if (!this->enabled) {
		return;
	}

	char buffer[BUFSIZE];
	int received;
	struct sockaddr_in remaddr;

	// loop through packets received
	do {
		received = getNextMessage(buffer, &remaddr, sizeof(buffer));
		if (received > 0) {
			char opcode = buffer[0];
			std::string msg = std::string(buffer + 1, received - 1);

			if (opcode == SONG && this->waitingForSongChanges) {
				this->songWaitingToBeChangedTo = msg;
			} else if (opcode == START && this->waitingForSynchronizedStarting) {
				this->shouldStart = true;
			} else if (opcode == SCORE) {
				this->receiveScoreChange(remaddr.sin_addr, msg);
			}
		}
	} while (received > 0);
}

std::vector<std::pair<ScoreKey, ScoreValue>> SyncStartManager::getPlayerScores() {
	std::vector<std::pair<ScoreKey, ScoreValue>> result;

	for (auto i = playerScores.begin(); i != playerScores.end(); i++) {
		result.push_back(*i);
	}

	std::sort(result.begin(), result.end(), sortScorePairs);
	return result;
}

void SyncStartManager::ListenForSongChanges(bool enabled) {
	LOG->Info("Listen for song changes: %d", enabled);
	this->waitingForSongChanges = enabled;
	this->songWaitingToBeChangedTo = "";
}

std::string SyncStartManager::ShouldChangeSong() {
	std::string song = this->songWaitingToBeChangedTo;
	this->songWaitingToBeChangedTo = "";
	return song;
}

void SyncStartManager::ListenForSynchronizedStarting(bool enabled) {
	LOG->Info("Listen for synchronized starting: %d", enabled);
	this->waitingForSynchronizedStarting = enabled;
	this->shouldStart = false;
}

bool SyncStartManager::ShouldStart() {
	bool shouldStart = this->shouldStart;
	this->shouldStart = false;
	return shouldStart;
}

// lua start
#include "LuaBinding.h"

class LunaSyncStartManager: public Luna<SyncStartManager> {
	public:
		static int GetCurrentPlayerScores( T* p, lua_State *L )
		{
			auto scores = p->getPlayerScores();
			lua_newtable( L );
			int outer_table_index = lua_gettop(L);

			int i = 0;
			for (auto score = scores.begin(); score != scores.end(); score++) {
				lua_newtable( L );
				int inner_table_index = lua_gettop(L);
				
				lua_pushstring(L, "playerName");
				lua_pushstring(L, score->first.playerName.c_str());
				lua_settable(L, inner_table_index);
 
				lua_pushstring(L, "score");
				lua_pushnumber(L, score->second.score);
				lua_settable(L, inner_table_index);
				
				lua_rawseti(L, outer_table_index, i + 1);
				i++;
			}

			return 1;
		}

		LunaSyncStartManager()
		{
			ADD_METHOD( GetCurrentPlayerScores );
		}
};

LUA_REGISTER_CLASS( SyncStartManager )
