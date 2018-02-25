#ifndef SRC_SYNCSTARTMANAGER_H_
#define SRC_SYNCSTARTMANAGER_H_

#include <string>
#include <arpa/inet.h>
#include "PlayerNumber.h"

struct ScoreEntry {
  // ids
  std::string machine;
  PlayerNumber player;

  // data
  std::string playerName;
  float score;
};

class SyncStartManager
{
private:
	int socketfd;
	bool enabled;
	void broadcast(std::string msg);
	int getNextMessage(char* buffer, sockaddr_in* remaddr, size_t bufferSize);
	void handleScoreUpdate(std::string message);

	bool waitingForSongChanges;
	std::string songWaitingToBeChangedTo;

	bool waitingForSynchronizedStarting;
	bool shouldStart;

	std::vector<ScoreEntry> scores;
public:
	SyncStartManager();
	~SyncStartManager();
	bool isEnabled();
	void enable();
	void disable();
	void broadcastStarting();
	void broadcastSongPath(std::string songPath);
	void broadcastScoreChange(std::string playerName, PlayerNumber pn, float scorePercentage);

	void Update();
	void ListenForSongChanges(bool enabled);
	std::string ShouldChangeSong();
	void ListenForSynchronizedStarting(bool enabled);
	bool ShouldStart();
};

extern SyncStartManager *SYNCMAN;

#endif /* SRC_SYNCSTARTMANAGER_H_ */
