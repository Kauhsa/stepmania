#include <algorithm>

#include "global.h"
#include "SyncStartScoreKeeper.h"
#include "RageLog.h"
#include "NoteTypes.h"

const int MAX_NOTE_ROW_DIFFERENCE_ALLOWED = ROWS_PER_BEAT;
const int BUFFER_SIZE = 10;

bool operator<(const ScorePlayer& l, const ScorePlayer& r) {
	return std::tie(l.machineAddress.s_addr, l.playerNumber) < std::tie(r.machineAddress.s_addr, r.playerNumber);
}

bool sortScoresByScore(const SyncStartScore& l, const SyncStartScore& r) {
	return r.data.score < l.data.score;
}

bool SyncStartScoreKeeper::AddScore(const ScorePlayer scorePlayer, int noteRow, const ScoreData scoreValue) {
	auto& scoresForPlayer = scoreBuffer[scorePlayer];

	// if we received noterow that is *less* then the current max, use that as
	// use the current max as noterow. this can happen because, for example,
	// mines are often judged very late and other judgements may have been
	// happened previously.
	noteRow = scoresForPlayer.empty()
		? noteRow
		: std::max(scoresForPlayer.rbegin()->first, noteRow);
	
	scoresForPlayer[noteRow] = scoreValue;

	// update largest note row
	this->largestNoteRow = std::max(noteRow, this->largestNoteRow);
	
	// calculate again what is the best note row we can use
	this->usedNoteRow = INT_MAX;
	for (auto scoreBufferIter = scoreBuffer.begin(); scoreBufferIter != scoreBuffer.end(); scoreBufferIter++) {
		if (scoreBufferIter->second.empty()) {
			continue;
		}

		int largestNoteRowForPlayer = scoreBufferIter->second.rbegin()->first;
		if (largestNoteRowForPlayer < this->usedNoteRow && largestNoteRow - largestNoteRowForPlayer < MAX_NOTE_ROW_DIFFERENCE_ALLOWED) {
			this->usedNoteRow = largestNoteRowForPlayer;
		}
	}

	// clean extra rows
	if (scoresForPlayer.size() > BUFFER_SIZE) {
		auto clearBegin = scoresForPlayer.begin();
		auto clearEnd = scoresForPlayer.begin();
		std::advance(clearEnd, scoresForPlayer.size() - BUFFER_SIZE);
		scoresForPlayer.erase(clearBegin, clearEnd);
	}

	#ifdef DEBUG
		std::stringstream ss;
		for (auto iter = scoresForPlayer.begin(); iter != scoresForPlayer.end(); iter++) {
			if (iter != scoresForPlayer.begin()) {
				ss << ",";
			}
			ss << iter->first;
		}
		LOG->Info("Note rows stored %d: {%s}", scorePlayer.playerNumber, ss.str().c_str());
	#endif

	// if the new score has same note row than used note row, then the scores have changed.
	return noteRow == this->usedNoteRow;
}

std::vector<SyncStartScore> SyncStartScoreKeeper::GetScores(bool latestValues) {
	std::vector<SyncStartScore> scores;
	scores.reserve(scoreBuffer.size());

	#ifdef DEBUG
		LOG->Info("Finding scores, note row %d", this->usedNoteRow);
	#endif

	for (auto iter = scoreBuffer.begin(); iter != scoreBuffer.end(); iter++) {
		auto scoreForCurrentRow = latestValues ? 
			iter->second.end() :
			iter->second.upper_bound(this->usedNoteRow);
		
		if (scoreForCurrentRow != iter->second.begin()) {
			scoreForCurrentRow--;
		}
		
		if (scoreForCurrentRow != iter->second.end()) {
			scores.emplace_back(SyncStartScore {
				.player = iter->first,
				.data = scoreForCurrentRow->second
			});
		}
	}

	std::sort(scores.begin(), scores.end(), sortScoresByScore);
	return scores;
}

void SyncStartScoreKeeper::ResetScores() {
	this->usedNoteRow = 0;
	this->largestNoteRow = 0;
	this->scoreBuffer.clear();
}

void SyncStartScoreKeeper::ResetButKeepScores() {
	this->usedNoteRow = 0;
	this->largestNoteRow = 0;

	for (auto iter = scoreBuffer.begin(); iter != scoreBuffer.end(); iter++) {
		auto reverseScoreIterForPlayer = iter->second.rbegin();
		
		if (reverseScoreIterForPlayer != iter->second.rend()) {
			auto latestScore = reverseScoreIterForPlayer->second;
			iter->second.clear();
			iter->second[0] = latestScore;
		} else {
			iter->second.clear();
		}
	}
}
