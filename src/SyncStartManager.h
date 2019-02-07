#ifndef SRC_SYNCSTARTMANAGER_H_
#define SRC_SYNCSTARTMANAGER_H_

#include <string>
#include <arpa/inet.h>

#include "PlayerNumber.h"
#include "PlayerStageStats.h"
#include "SyncStartScoreKeeper.h"
#include "Song.h"
#include "Course.h"

class SyncStartManager
{
private:
	int socketfd;
	bool enabled;
	void broadcast(char code, const std::string& msg);
	int getNextMessage(char* buffer, sockaddr_in* remaddr, size_t bufferSize);

	bool waitingForSongChanges;
	std::string songOrCourseWaitingToBeChangedTo;
	bool waitingForSynchronizedStarting;
	std::string activeSyncStartSong;
	bool shouldStart;

	SyncStartScoreKeeper syncStartScoreKeeper;
public:
	SyncStartManager();
	~SyncStartManager();
	bool isEnabled();
	void enable();
	void disable();
	void broadcastStarting();
	void broadcastSelectedSong(const Song& song);
	void broadcastSelectedCourse(const Course& course);
	void broadcastScoreChange(const PlayerStageStats& pPlayerStageStats);
	void receiveScoreChange(struct in_addr in_addr, const std::string& msg);
	std::vector<SyncStartScore> GetCurrentPlayerScores();
	std::vector<SyncStartScore> GetLatestPlayerScores();
	void Update();
	void ListenForSongChanges(bool enabled);
	std::string GetSongOrCourseToChangeTo();
	void StartListeningForSynchronizedStart(const Song& song);
	void StopListeningForSynchronizedStart();
	bool AttemptStart();
	void StopListeningScoreChanges();
	void SongChangedDuringGameplay(const Song& song);

	// Lua
	void PushSelf( lua_State *L );
};

extern SyncStartManager *SYNCMAN;

#endif /* SRC_SYNCSTARTMANAGER_H_ */
