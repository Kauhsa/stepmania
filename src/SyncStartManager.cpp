// idk what I need exactly, this should be ok
#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string>
#include <algorithm>

#include "global.h"
#include "SyncStartManager.h"
#include "ScreenManager.h"
#include "SongManager.h"
#include "ScreenSelectMusic.h"

SyncStartManager *SYNCMAN;

#define BUFSIZE 1024
#define PORT 53000

// opcodes

#define START 'S'
#define SONG 'W'

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

	if (bind(this->socketfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		return;
	}

	this->enabled = true;
}

void SyncStartManager::broadcast(std::string msg) {
	if (!this->enabled) {
		return;
	}

	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd == -1) {
		return;
	}

	int broadcast = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) == -1) {
	    return;
	}

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(PORT);
	addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

	const char* content = msg.c_str();

	if (sendto(fd, content, msg.size(), 0, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
		return;
	}

	shutdown(fd, SHUT_RDWR);
	close(fd);
}

void SyncStartManager::broadcastStarting()
{
	this->broadcast(std::string(1, START));
}

void SyncStartManager::broadcastSongPath(std::string songPath) {
	this->broadcast(SONG + songPath);
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

int SyncStartManager::getNextMessage(char* buffer, size_t bufferSize) {
	struct sockaddr_in remaddr;
	socklen_t addrlen = sizeof remaddr;
	ssize_t received;
	return recvfrom(this->socketfd, buffer, bufferSize, MSG_DONTWAIT, (struct sockaddr *) &remaddr, &addrlen);
}

std::string SyncStartManager::shouldChangeSong()
{
	if (!this->enabled) {
		return "";
	}

	char buffer[BUFSIZE];
	int received;

	// loop through packets, ignoring everything other than song change
	do {
		received = getNextMessage(buffer, sizeof(buffer));
		if (received > 0 && buffer[0] == SONG) {
			return std::string(buffer + 1, received - 1);
		}
	} while (received > 0);

	return "";
}

bool SyncStartManager::shouldStart()
{
	if (!this->enabled) {
		return true;
	}

	// care only about first byte here, so discard rest
	char buffer[1];
	int received;

	// loop through packets, ignoring everything else until we hit start message
	do {
		received = getNextMessage(buffer, sizeof(buffer));
		if (received > 0 && buffer[0] == START) {
			return true;
		}
	} while (received > 0);

	return false;
}
