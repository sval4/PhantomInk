#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <string.h>
#include "gamedata.h"

#define PORT 8080
#define MAX_CLIENTS 4
#define MAX_TEAMS 2
#define DEBUG 0

void collectClients(int server_fd, struct sockaddr_in address, int client_sockets[]);
void broadcast(int client_sockets[], char* broadcast_msg);
void closeClients(int client_sockets[]);
void assignTeams(int client_sockets[], int** teams);
void freeTeams(int** teams);
char* createEmptyBoard();
void broadcastToWriters(int** teams, char* broadcast_msg);
void assignWords(char** secret_word);
void assignQuestions(char* team_questions[MAX_TEAMS][MAX_QUESTIONS]);
void broadcastToGuessers(int** teams, int teamNum, char* broadcast_msg);
void listenToGuessers(int** teams, int teamNum, char* team_questions[MAX_TEAMS][MAX_QUESTIONS]);
char listenToWriters(int** teams, int teamNum);

int main() {
    char* secret_word;
    char* team_questions[MAX_TEAMS][MAX_QUESTIONS];
    int server_fd;
    int client_sockets[MAX_CLIENTS];
    int** teams = (int**) malloc(MAX_TEAMS * sizeof(int*));
    struct sockaddr_in address;
    char broadcast_msg[] = "Welcome to Phantom Ink!!!\n";
    char* board = createEmptyBoard();
    int boardPos = -1;
    char writerMessage[] = "You have been assigned as the writer\n";
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
    assignWords(&secret_word);
    assignQuestions(team_questions);

    boardPos = 20;
    if(DEBUG != 1){
        broadcast(client_sockets, board);
        broadcastToWriters(teams, writerMessage);
        for(int i = 0; i < MAX_TEAMS; i++){
            char word_msg[100];
            snprintf(word_msg, sizeof(word_msg), "The word is %s\n",secret_word);
            send(teams[i][0], word_msg, strlen(word_msg), 0);
        }
        for(int i = 0; i < MAX_TEAMS; i++){
            printf("\n>>> STARTING TURN: Team %d <<<\n", i);
            char turn_msg[100];
            snprintf(turn_msg, sizeof(turn_msg), "\n--- TEAM %d: YOUR TURN ---\n", i + 1);
            broadcastToGuessers(teams, i, turn_msg);
            for(int j = 0; j < MAX_QUESTIONS; j++){
                char formatted_msg[2048]; 
                snprintf(formatted_msg, sizeof(formatted_msg), "%d) %s\n", j + 1, team_questions[i][j]);
                printf("Broadcast questions to guessers Team: %d\n", i);
                broadcastToGuessers(teams,i,formatted_msg);
            }
            listenToGuessers(teams,i,team_questions);
            char letter = listenToWriters(teams,i);
            board[boardPos] = letter;
            broadcast(client_sockets,board);
            // Implement guesser saying whether writer needs to continue or stop
            // Remove question and put a new one after it is used
            //Broadcast assigned words to writers

            //Move to next team line
            boardPos = 36;
        }
        boardPos = 49; //Goes to start of second line for Team 0
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

void assignWords(char** secret_word){
    int index;
    index = rand() % LENGTH_WORDS;
    *secret_word = PHANTOM_WORDS[index];
    printf("Secret Word: %s\n", *secret_word);
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
            team_pos_index += 1;
            current_team_size += num_per_team + (team_index < leftover ? 1 : 0);
        }
        printf("Team Index: %d, Client Num: %d\n", team_index, c);
    }
    printf("\n");
}

char listenToWriters(int** teams, int teamNum) {
    char buffer[1024];
    char letter = '\0';
    char* prompt="Please enter a Letter: ";
    char* error;

    while (letter < 'A' || letter > 'Z') {
        memset(buffer, 0, sizeof(buffer));
        send(teams[teamNum][0], prompt, strlen(prompt), 0);

        int valread = recv(teams[teamNum][0], buffer, sizeof(buffer) - 1, 0);
        
        if (valread <= 0) continue;
        if (strlen(buffer) != 2) continue;

        letter = buffer[0];

        if (letter >= 'a' && letter <= 'z') {
            letter -= 32; 
        }

        if (letter >= 'A' && letter <= 'Z') {
            printf("Team %d Writer sent letter: %c\n", teamNum, letter);
            char msg[64];
            snprintf(msg, sizeof(msg), "The Spirit writes: %c\n", letter);
            printf("%s",msg);
            return letter;
        } else {
            error = "Invalid! Send a single letter (A-Z).\n";
            send(teams[teamNum][0], error, strlen(error), 0);
        }
    }
}

void listenToGuessers(int** teams, int teamNum, char* team_questions[MAX_TEAMS][MAX_QUESTIONS]) {
    char buffer[1024];
    int choice = -1;
    int num_per_team = MAX_CLIENTS / MAX_TEAMS;
    int leftover = MAX_CLIENTS % MAX_TEAMS;
    int current_team_size = num_per_team + (teamNum < leftover ? 1 : 0);
    char* error;

    // Loop until we get a valid choice (0-4 because of choice -= 1)
    while (choice < 0 || choice >= MAX_QUESTIONS) {
        broadcastToGuessers(teams,teamNum,"Please Select a Question (1-5): ");
        fd_set readfds;
        FD_ZERO(&readfds);
        int max_sd = 0;

        for (int i = 1; i < current_team_size; i++) {
            int sd = teams[teamNum][i];
            FD_SET(sd, &readfds);
            if (sd > max_sd) max_sd = sd;
        }

        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0) { perror("select error"); return; }

        for (int i = 1; i < current_team_size; i++) {
            int sd = teams[teamNum][i];
            if (FD_ISSET(sd, &readfds)) {
                int valread = recv(sd, buffer, sizeof(buffer) - 1, 0);
                if (valread <= 0) continue; // Handle disconnects or empty reads

                buffer[valread] = '\0';
                
                // atoi returns 0 for non-numeric strings, but 0 is invalid anyway
                int input_val = atoi(buffer); 

                if (input_val >= 1 && input_val <= MAX_QUESTIONS) {
                    choice = input_val - 1; 
                    char msg[1048];
                    printf("Team %d chose question: %s\n", teamNum, team_questions[teamNum][choice]);
                    snprintf(msg, sizeof(msg), "%s\n", team_questions[teamNum][choice]);
                    send(teams[teamNum][0], msg, strlen(msg), 0);
                    return;
                } else {
                    error = "Invalid choice! Please enter a number between 1 and 5.\n";
                    send(sd, error, strlen(error), 0);
                }
            }
        }
    }
}

void broadcastToGuessers(int** teams, int teamNum, char* broadcast_msg) {
    int num_per_team = MAX_CLIENTS / MAX_TEAMS;
    int leftover = MAX_CLIENTS % MAX_TEAMS;

    int current_team_size = num_per_team + (teamNum < leftover ? 1 : 0);
    for (int j = 1; j < current_team_size; j++) {
        send(teams[teamNum][j], broadcast_msg, strlen(broadcast_msg), 0);
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
    broadcast(client_sockets, "Thanks for Playing!!!");
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

