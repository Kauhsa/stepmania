#include <algorithm>

#include "global.h"
#include "SyncStartScoreKeeper.h"
#include "RageLog.h"
#include "NoteTypes.h"

#define MAX_POSSIBLE_DANCE_POINTS_DIFFERENCE 100

bool operator<(const ScorePlayer& l, const ScorePlayer& r) {
	return std::tie(l.machineAddress.s_addr, l.playerNumber) < std::tie(r.machineAddress.s_addr, r.playerNumber);
}

float getScore(const SyncStartScore& score) {
	if (score.data.possibleDancePoints == 0) {
		return 0;
	}

	return clamp(0, score.data.actualDancePoints / score.data.possibleDancePoints, 1);
}

float getLostScore(const SyncStartScore& score) {
	if (score.data.currentPossibleDancePoints == 0) {
		return 0;
	}

	return clamp(0, score.data.actualDancePoints / score.data.currentPossibleDancePoints, 1);
}

bool sortByScore(const SyncStartScore& l, const SyncStartScore& r) {
	auto rScore = getScore(r);
	auto lScore = getScore(l);
	auto rFailed = r.data.failed ? 0 : 1;
	auto lFailed = l.data.failed ? 0 : 1;
	return std::tie(rFailed, rScore) < std::tie(lFailed, lScore);
}

bool sortByLostScore(const SyncStartScore& l, const SyncStartScore& r) {
	// if a connection to a machine is dropped or something, don't keep it's position by lost score
	if (abs(l.data.currentPossibleDancePoints - r.data.currentPossibleDancePoints) > MAX_POSSIBLE_DANCE_POINTS_DIFFERENCE) {
		return sortByScore(l, r);
	}

	auto rScore = getLostScore(r);
	auto lScore = getLostScore(l);
	auto rFailed = r.data.failed ? 0 : 1;
	auto lFailed = l.data.failed ? 0 : 1;
	return std::tie(rFailed, rScore) < std::tie(lFailed, lScore);
}

void SyncStartScoreKeeper::AddScore(const ScorePlayer scorePlayer, const ScoreData scoreData) {
	scoreBuffer[scorePlayer] = scoreData;
}

std::vector<SyncStartScore> SyncStartScoreKeeper::GetScores(bool latestValues) {
	std::vector<SyncStartScore> scores;
	scores.reserve(scoreBuffer.size());

	for (auto iter = scoreBuffer.begin(); iter != scoreBuffer.end(); iter++) {
		scores.emplace_back(SyncStartScore {
			.player = iter->first,
			.data = iter->second
		});
	}

	std::sort(scores.begin(), scores.end(), latestValues ? sortByScore : sortByLostScore);
	return scores;
}

void SyncStartScoreKeeper::ResetScores() {
	this->scoreBuffer.clear();
}

