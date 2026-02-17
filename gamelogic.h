#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <stdbool.h>
#include "gamedata.h"

// --- Configuration Constants ---
#define PORT 8080
#define MAX_CLIENTS 4
#define MAX_TEAMS 2
#define DEBUG 0

// --- Function Prototypes ---
void collectClients(int server_fd, struct sockaddr_in address, int client_sockets[]);
void broadcast(int client_sockets[], char* broadcast_msg);
void closeClients(int client_sockets[], int teamNum);
void assignTeams(int client_sockets[], int** teams);
void freeTeams(int** teams);
char* createEmptyBoard(size_t* boardPos, size_t* nextLinePos, size_t* newLinePos);
void broadcastToWriters(int** teams, char* broadcast_msg);
void assignWords(char** secret_word);
void assignQuestions(char* team_questions[MAX_TEAMS][MAX_QUESTIONS], int* deckIndex);
void broadcastToGuessers(int** teams, int teamNum, char* broadcast_msg);
char listenToGuessers(int** teams, int teamNum, char* team_questions[MAX_TEAMS][MAX_QUESTIONS], bool askedQuestion, int* guesserSocket);
char listenToWriter(int sd, bool isGuesser);
void cleanUp(int server_fd, int client_sockets[], int** teams, char* board);

#endif