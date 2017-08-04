#ifndef SRC_SYNCSTARTMANAGER_H_
#define SRC_SYNCSTARTMANAGER_H_

#include <string>

class SyncStartManager
{
private:
	int socketfd;
	bool enabled;
	void broadcast(std::string msg);
	int getNextMessage(char* buffer, size_t bufferSize);
public:
	SyncStartManager();
	~SyncStartManager();
	bool isEnabled();
	void enable();
	void disable();
	void cleanup();
	void broadcastStarting();
	void broadcastSongPath(std::string songPath);
	std::string shouldChangeSong();
	bool shouldStart();
};

extern SyncStartManager *SYNCMAN;

#endif /* SRC_SYNCSTARTMANAGER_H_ */
