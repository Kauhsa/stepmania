#ifndef SRC_SYNCSTARTSCOREKEEPER_H
#define SRC_SYNCSTARTSCOREKEEPER_H

#include <string>
#include <arpa/inet.h>

#include "PlayerNumber.h"
#include "PlayerStageStats.h"


struct ScorePlayer {
	struct in_addr machineAddress;
	PlayerNumber playerNumber;
	std::string playerName;
};

struct ScoreData {
	float score;
	float life;
	bool failed;
	int tapNoteScores[NUM_TapNoteScore];
	int holdNoteScores[NUM_HoldNoteScore];
};

struct SyncStartScore {
    ScorePlayer player;
    ScoreData data;
};

class SyncStartScoreKeeper {
    private:
        std::map<ScorePlayer, std::map<int, ScoreData>> scoreBuffer;
        int largestNoteRow = 0;
        int usedNoteRow = 0;

    public:
        bool AddScore(const ScorePlayer scorePlayer, int noteRow, const ScoreData scoreData);
        std::vector<SyncStartScore> GetScores(bool latestValues);
        void ResetScores();
        void ResetButKeepScores();
};

#endif /* SRC_SYNCSTARTSCOREKEEPER_H */
