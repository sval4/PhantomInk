#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <string.h>
#include "gamedata.h"

#define PORT 8080
#define MAX_CLIENTS 3
#define MAX_TEAMS 2
#define DEBUG 0

void collectClients(int server_fd, struct sockaddr_in address, int client_sockets[]);
void broadcast(int client_sockets[], char* broadcast_msg);
void closeClients(int client_sockets[]);
void assignTeams(int client_sockets[], int** teams);
void freeTeams(int** teams);
char* createEmptyBoard();
void broadcastToWriters(int** teams, char* broadcast_msg);
void assignWords(char** team_words);
void assignQuestions(char* team_questions[MAX_TEAMS][MAX_QUESTIONS]);
void broadcastToGuessers(int** teams, int teamNum, char* broadcast_msg);

int main() {
    char* team_words[MAX_TEAMS];
    char* team_questions[MAX_TEAMS][MAX_QUESTIONS];
    int server_fd;
    int client_sockets[MAX_CLIENTS];
    int** teams = (int**) malloc(MAX_TEAMS * sizeof(int*));
    struct sockaddr_in address;
    char broadcast_msg[] = "Welcome to Phantom Ink!!!\n";
    char* board = createEmptyBoard();
    char writerMessage[] = "You have been assigned as the writer";
    srand(time(NULL));

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // This is here to prevent "Address already in use" errors when a restart occurs too quickly
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // --- Retrieve and Print Local IP ---
    struct sockaddr_in name;
    socklen_t namelen = sizeof(name);
    getsockname(server_fd, (struct sockaddr *)&name, &namelen);
    printf("Server listening on %s:%d\n", inet_ntoa(name.sin_addr), ntohs(name.sin_port));
    printf("Waiting for %d clients to connect...\n", MAX_CLIENTS);

    if(DEBUG != 1){
        collectClients(server_fd, address, client_sockets);
    }
    

    printf("\nAll clients connected. Sending broadcast message...\n");\
    if(DEBUG != 1){
        broadcast(client_sockets, broadcast_msg);
    }

    assignTeams(client_sockets, teams);
    assignWords(team_words);
    assignQuestions(team_questions);

    if(DEBUG != 1){
        broadcast(client_sockets, board);
    }
    
    if(DEBUG != 1){
        broadcastToWriters(teams, writerMessage);
    }

    if(DEBUG != 1){
        for(int i = 0; i < MAX_TEAMS; i++){
            for(int j = 0; j < MAX_QUESTIONS; j++){
                broadcastToGuessers(teams,i,team_questions[i][j]);
            }
        }
    }

    printf("%s",board);

    closeClients(client_sockets);

    freeTeams(teams);
    free(board);

    printf("\nSession complete. Shutting down server.\n");
    close(server_fd);

    return 0;
}

void assignQuestions(char* team_questions[MAX_TEAMS][MAX_QUESTIONS]){
    int index;
    char* temp;
    for(int i = 0; i < MAX_TEAMS; i++){
        for(int j = 0; j < MAX_QUESTIONS; j++){
            index = rand() % (LENGTH_QUESTIONS - j);
            team_questions[i][j] = PHANTOM_QUESTIONS[index];
            temp = PHANTOM_QUESTIONS[index];
            PHANTOM_QUESTIONS[index] = PHANTOM_QUESTIONS[LENGTH_QUESTIONS - j - 1];
            PHANTOM_QUESTIONS[LENGTH_QUESTIONS - j - 1] = temp;
            printf("Team %d Question %d: %s\n", i, j + 1, team_questions[i][j]);
        }
        printf("\n");
    }
}

void assignWords(char** team_words){
    int index;
    char* temp;
    for(int i = 0; i < MAX_TEAMS; i++){
        index = rand() % (LENGTH_WORDS - i);
        team_words[i] = PHANTOM_WORDS[index];
        temp = PHANTOM_WORDS[index];
        PHANTOM_WORDS[index] = PHANTOM_WORDS[LENGTH_WORDS - i - 1];
        PHANTOM_WORDS[LENGTH_WORDS - i - 1] = temp;
        printf("Team %d Secret Word: %s\n", i, team_words[i]);
    }
    printf("\n");
}

char* createEmptyBoard(){
    int numLines = 8;
    char* board = malloc(1024 * sizeof(char));
    if (board == NULL) return NULL;
    board[0] = '\0';
    strcat(board, "\t     Phantom Ink\t\t\n");
    for (int i = 0; i < numLines; i++) {
        strcat(board, "____________\t  \t____________\n");
    }
    strcat(board, "\n\n\n");
    return board;
}

void freeTeams(int** teams) {
    if (teams == NULL) return;

    for (int i = 0; i < MAX_TEAMS; i++) {
        if (teams[i] != NULL) {
            free(teams[i]);
        }
    }

    free(teams);
    printf("Team memory cleaned up successfully.\n");
}

void assignTeams(int client_sockets[], int** teams) {
    int temp_pool[MAX_CLIENTS];
    int new_size[MAX_TEAMS] = {0};
    int index;
    int current_team_size;

    memcpy(temp_pool, client_sockets, sizeof(temp_pool));

    for (int i = MAX_CLIENTS - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int temp = temp_pool[i];
        temp_pool[i] = temp_pool[j];
        temp_pool[j] = temp;
    }

    int num_per_team = MAX_CLIENTS / MAX_TEAMS;
    int leftover = MAX_CLIENTS % MAX_TEAMS;
    for(int t = 0; t < MAX_TEAMS; t++){
        current_team_size = num_per_team + (t < leftover ? 1 : 0);
        teams[t] = malloc(current_team_size * sizeof(int));
    }

    printf("Num per Team: %d, Leftover: %d\n", num_per_team, leftover);

    int team_index = 0;
    int team_pos_index = 0;
    current_team_size = num_per_team + (team_index < leftover ? 1 : 0);
    for (int c = 0; c < MAX_CLIENTS; c++){
        if(c < current_team_size){
            teams[team_index][team_pos_index] = temp_pool[c];
            team_pos_index += 1;
        }else{
            team_index += 1;
            team_pos_index = 0;
            teams[team_index][team_pos_index] = temp_pool[c];
            current_team_size += num_per_team + (team_index < leftover ? 1 : 0);
        }
        printf("Team Index: %d, Client Num: %d\n", team_index, c);
    }
    printf("\n");
}

void broadcastToGuessers(int** teams, int teamNum, char* broadcast_msg) {
    int num_per_team = MAX_CLIENTS / MAX_TEAMS;
    int leftover = MAX_CLIENTS % MAX_TEAMS;
    char formatted_msg[2048]; 

    snprintf(formatted_msg, sizeof(formatted_msg), "%s\n", broadcast_msg);

    int current_team_size = num_per_team + (teamNum < leftover ? 1 : 0);
    for (int j = 1; j < current_team_size; j++) {
        send(teams[teamNum][j], formatted_msg, strlen(formatted_msg), 0);
    }
}

void broadcastToWriters(int** teams, char* broadcast_msg){
    for (int i = 0; i < MAX_TEAMS; i++) {
        send(teams[i][0], broadcast_msg, strlen(broadcast_msg), 0);
    }
}

void broadcast(int client_sockets[], char* broadcast_msg){
    for (int i = 0; i < MAX_CLIENTS; i++) {
        send(client_sockets[i], broadcast_msg, strlen(broadcast_msg), 0);
    }
}

void closeClients(int client_sockets[]){
    for (int i = 0; i < MAX_CLIENTS; i++) {
        close(client_sockets[i]);
        printf("[Client %d] Disconnected.\n", i + 1);
    }
}

void collectClients(int server_fd, struct sockaddr_in address, int client_sockets[]){
    socklen_t addrlen = sizeof(address);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_sockets[i] = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (client_sockets[i] < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        
        printf("[Client %d] Connected from %s. (%d/%d)\n", 
                i + 1, inet_ntoa(address.sin_addr), i + 1, MAX_CLIENTS);
        
        char *wait_msg = "Connected! Waiting for others to join...\n";
        send(client_sockets[i], wait_msg, strlen(wait_msg), 0);
    }
}

