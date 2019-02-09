#ifndef SRC_SYNCSTARTMANAGER_H_
#define SRC_SYNCSTARTMANAGER_H_

#include <string>
#include <arpa/inet.h>

#include "PlayerNumber.h"
#include "PlayerStageStats.h"

struct ScoreKey {
	struct in_addr machineAddress;
	PlayerNumber playerNumber;
	std::string playerName;
};

bool operator<(const ScoreKey& l, const ScoreKey& r);

struct ScoreValue {
	int score;
	float life;
};

class SyncStartManager
{
private:
	int socketfd;
	bool enabled;
	void broadcast(char code, std::string msg);
	int getNextMessage(char* buffer, sockaddr_in* remaddr, size_t bufferSize);

	bool waitingForSongChanges;
	std::string songWaitingToBeChangedTo;

	bool waitingForSynchronizedStarting;
	bool shouldStart;

	std::map<ScoreKey, ScoreValue> playerScores;
public:
	SyncStartManager();
	~SyncStartManager();
	bool isEnabled();
	void enable();
	void disable();
	void broadcastStarting();
	void broadcastSongPath(std::string songPath);
	void broadcastScoreChange(int noteRow, const PlayerStageStats& pPlayerStageStats);
	void receiveScoreChange(struct in_addr in_addr, const std::string& msg);
	std::vector<std::pair<ScoreKey, ScoreValue>> getPlayerScores();

	void Update();
	void ListenForSongChanges(bool enabled);
	std::string ShouldChangeSong();
	void ListenForSynchronizedStarting(bool enabled);
	bool ShouldStart();

	// Lua
	void PushSelf( lua_State *L );
};

extern SyncStartManager *SYNCMAN;

#endif /* SRC_SYNCSTARTMANAGER_H_ */
