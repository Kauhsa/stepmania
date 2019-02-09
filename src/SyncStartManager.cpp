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

SyncStartManager::SyncStartManager()
{
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

void SyncStartManager::broadcastScoreChange(int noteRow, const PlayerStageStats *pPlayerStageStats) {
	std::string msg = PROFILEMAN->GetPlayerName(pPlayerStageStats->m_player_number)
		+ '|'
		+ std::to_string(noteRow)
		+ '|'
		+ std::to_string(pPlayerStageStats->GetPercentDancePoints())
		+ '|'
		+ std::to_string(pPlayerStageStats->GetCurrentLife());

	this->broadcast(SCORE, msg); 
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
	return recvfrom(this->socketfd, buffer, bufferSize, MSG_DONTWAIT, (struct sockaddr *) &remaddr, &addrlen);
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
				//
			}
		}
	} while (received > 0);
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
