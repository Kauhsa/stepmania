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
#include "GameState.h"
#include "CommonMetrics.h"

SyncStartManager *SYNCMAN;

#define BUFSIZE 1024
#define PORT 53000

// opcodes

#define START 0x00
#define SONG 0x01
#define SCORE 0x02

#define MISC_ITEMS_LENGTH 9
#define ALL_ITEMS_LENGTH (MISC_ITEMS_LENGTH + NUM_TapNoteScore + NUM_HoldNoteScore)

std::vector<std::string> split(const std::string& str, const std::string& delim) {
	std::vector<std::string> tokens;
	tokens.reserve(ALL_ITEMS_LENGTH);
	size_t prev = 0, pos = 0;
	do
	{
		pos = str.find(delim, prev);
		if (pos == std::string::npos) pos = str.length();
		std::string token = str.substr(prev, pos-prev);
		tokens.push_back(token);
		prev = pos + delim.length();
	}
	while (pos < str.length() && prev < str.length());
	return tokens;
}

std::string SongToString(const Song& song) {
	RString sDir = song.GetSongDir();
	sDir.Replace("\\","/");
	vector<RString> bits;
	split(sDir, "/", bits);
	return song.m_sGroupName + '/' + *bits.rbegin();
}

std::string formatScore(const PlayerStageStats& pPlayerStageStats) {
	return ssprintf("%.*f", 
			(int) CommonMetrics::PERCENT_SCORE_DECIMAL_PLACES,
			pPlayerStageStats.GetPercentDancePoints() * 100 );
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
	int enableOpt = 1;
	if (
		setsockopt(this->socketfd, SOL_SOCKET, SO_BROADCAST, &enableOpt, sizeof(enableOpt)) == -1 ||
		setsockopt(this->socketfd, SOL_SOCKET, SO_REUSEADDR, &enableOpt, sizeof(enableOpt)) == -1 ||
		setsockopt(this->socketfd, SOL_SOCKET, SO_REUSEPORT, &enableOpt, sizeof(enableOpt)) == -1
	) {
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

	#ifdef DEBUG
		LOG->Info("BROADCASTING: code %d, msg: '%s'", code, msg.c_str());
	#endif

	if (sendto(this->socketfd, &buffer, length + 1, 0, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
		return;
	}
}

void SyncStartManager::broadcastStarting()
{
	if (!this->activeSyncStartSong.empty()) {
		this->broadcast(START, this->activeSyncStartSong);
	}
}

void SyncStartManager::broadcastSongPath(Song& song) {
	this->broadcast(SONG, SongToString(song));
}

void SyncStartManager::broadcastScoreChange(const PlayerStageStats& pPlayerStageStats) {
	stringstream msg;

	std::string playerName = PROFILEMAN->GetPlayerName(pPlayerStageStats.m_player_number);
	if (playerName.empty()) {
		playerName = "NoName";
	}

	msg << this->activeSyncStartSong << '|';
	msg << (int) pPlayerStageStats.m_player_number << '|';
	msg << playerName << '|';
	msg << pPlayerStageStats.m_iActualDancePoints << '|';
	msg << pPlayerStageStats.m_iCurPossibleDancePoints << '|';
	msg << pPlayerStageStats.m_iPossibleDancePoints << '|';
	msg << formatScore(pPlayerStageStats) << '|';
	msg << pPlayerStageStats.GetCurrentLife() << '|';
	msg << (pPlayerStageStats.m_bFailed ? '1' : '0') << '|';

	for (int i = 0; i < NUM_TapNoteScore; ++i) {
		msg << pPlayerStageStats.m_iTapNoteScores[i] << '|';
	}

	for (int i = 0; i < NUM_HoldNoteScore; ++i) {
		msg << pPlayerStageStats.m_iHoldNoteScores[i];
		if (i != NUM_HoldNoteScore - 1) {
			msg << '|';
		}
	}

	this->broadcast(SCORE, msg.str()); 
}

void SyncStartManager::receiveScoreChange(struct in_addr in_addr, const std::string& msg) {
	if (this->activeSyncStartSong.empty()) {
		return;
	}
	
	ScorePlayer scorePlayer = {
		.machineAddress = in_addr
	};

	ScoreData scoreData;
	int noteRow;

	try {
		vector<std::string> items = split(msg, "|");
		
		// ignore messages that don't fit the size the message should be
		if (items.size() != ALL_ITEMS_LENGTH) {
			return;
		}

		auto iter = items.begin();

		// ignore scores for other than current song
		std::string& songName = *iter++;
		if (songName != this->activeSyncStartSong) {
			return;
		}

		scorePlayer.playerNumber = (PlayerNumber) std::stoi(*iter++); 
		scorePlayer.playerName = *iter++;
		scoreData.actualDancePoints = std::stoi(*iter++);
		scoreData.currentPossibleDancePoints = std::stoi(*iter++);
		scoreData.possibleDancePoints = std::stoi(*iter++);
		scoreData.formattedScore = *iter++;
		scoreData.life = std::stof(*iter++);
		scoreData.failed = *iter++ == "1" ? true : false;

		for (int i = 0; i < NUM_TapNoteScore; ++i) {
			scoreData.tapNoteScores[i] = std::stoi(*iter++);
		}

		for (int i = 0; i < NUM_HoldNoteScore; ++i) {
			scoreData.holdNoteScores[i] = std::stoi(*iter++);
		}

		this->syncStartScoreKeeper.AddScore(scorePlayer, scoreData);
		MESSAGEMAN->Broadcast("SyncStartPlayerScoresChanged");
	} catch (std::exception& e) {
		// just don't crash!
		LOG->Warn("Could not parse score change '%s'", msg);
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
				if (msg == activeSyncStartSong) {
					this->shouldStart = true;
				}
			} else if (opcode == SCORE) {
				this->receiveScoreChange(remaddr.sin_addr, msg);
			}
		}
	} while (received > 0);
}

std::vector<SyncStartScore> SyncStartManager::GetCurrentPlayerScores() {
	return this->syncStartScoreKeeper.GetScores(false);
}

std::vector<SyncStartScore> SyncStartManager::GetLatestPlayerScores() {
	return this->syncStartScoreKeeper.GetScores(true);
}

void SyncStartManager::ListenForSongChanges(bool enabled) {
	LOG->Info("Listen for song changes: %d", enabled);
	this->waitingForSongChanges = enabled;
	this->songWaitingToBeChangedTo = "";
}

std::string SyncStartManager::ShouldChangeSong() {
	std::string songToBeChangedTo = this->songWaitingToBeChangedTo;

	if (!songToBeChangedTo.empty()) {
		this->songWaitingToBeChangedTo = "";
		return songToBeChangedTo;
	} else {
		return "";
	}
}

void SyncStartManager::StartListeningForSynchronizedStart(const Song& song) {
	this->syncStartScoreKeeper.ResetScores();
	this->activeSyncStartSong = SongToString(song);
	this->shouldStart = false;
	this->waitingForSynchronizedStarting = true;
}

void SyncStartManager::StopListeningForSynchronizedStart() {
	this->shouldStart = false;
	this->waitingForSynchronizedStarting = false;
}

bool SyncStartManager::AttemptStart() {
	bool shouldStart = this->shouldStart;
	this->shouldStart = false;
	return shouldStart;
}

void SyncStartManager::SongChangedDuringGameplay(const Song& song) {
	this->activeSyncStartSong = SongToString(song);
}

void SyncStartManager::StopListeningScoreChanges() {
	this->activeSyncStartSong = "";
}

// lua start
#include "LuaBinding.h"

class LunaSyncStartManager: public Luna<SyncStartManager> {
	public:
		static void PushScores( T* p, lua_State *L, const std::vector<SyncStartScore>& scores )
		{
			lua_newtable( L );
			int outer_table_index = lua_gettop(L);

			int i = 0;
			for (auto score = scores.begin(); score != scores.end(); score++) {
				lua_newtable( L );
				int inner_table_index = lua_gettop(L);
				
				lua_pushstring(L, "playerName");
				lua_pushstring(L, score->player.playerName.c_str());
				lua_settable(L, inner_table_index);
 
				lua_pushstring(L, "score");
				lua_pushstring(L, score->data.formattedScore.c_str());
				lua_settable(L, inner_table_index);

				lua_pushstring(L, "failed");
				lua_pushboolean(L, score->data.failed);
				lua_settable(L, inner_table_index);
				
				lua_rawseti(L, outer_table_index, i + 1);
				i++;
			}
		}

		static int IsEnabled( T* p, lua_State *L )
		{
			lua_pushboolean(L, p->isEnabled());
			return 1;
		}

		static int GetCurrentPlayerScores( T* p, lua_State *L )
		{
			auto scores = p->GetCurrentPlayerScores();
			PushScores(p, L, scores);
			return 1;
		}

		static int GetLatestPlayerScores( T* p, lua_State *L )
		{
			auto scores = p->GetLatestPlayerScores();
			PushScores(p, L, scores);
			return 1;
		}

		LunaSyncStartManager()
		{
			ADD_METHOD( IsEnabled );
			ADD_METHOD( GetCurrentPlayerScores );
			ADD_METHOD( GetLatestPlayerScores );
		}
};

LUA_REGISTER_CLASS( SyncStartManager )
