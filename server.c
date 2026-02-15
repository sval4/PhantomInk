#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>
#include "gamedata.h"

#define PORT 8080
#define MAX_CLIENTS 4
#define MAX_TEAMS 2
#define DEBUG 0
// nc 0.0.0.0 8080


void collectClients(int server_fd, struct sockaddr_in address, int client_sockets[]);
void broadcast(int client_sockets[], char* broadcast_msg);
void closeClients(int client_sockets[], int teamNum);
void assignTeams(int client_sockets[], int** teams);
void freeTeams(int** teams);
char* createEmptyBoard();
void broadcastToWriters(int** teams, char* broadcast_msg);
void assignWords(char** secret_word);
void assignQuestions(char* team_questions[MAX_TEAMS][MAX_QUESTIONS], int* deckIndex);
void broadcastToGuessers(int** teams, int teamNum, char* broadcast_msg);
char listenToGuessers(int** teams, int teamNum, char* team_questions[MAX_TEAMS][MAX_QUESTIONS], bool askedQuestion, int* guesserSocket);
char listenToWriter(int sd, int teamNum, bool isGuesser);

int main() {
    char* secret_word;
    char* team_questions[MAX_TEAMS][MAX_QUESTIONS];
    int server_fd;
    int client_sockets[MAX_CLIENTS];
    int** teams = (int**) malloc(MAX_TEAMS * sizeof(int*));
    struct sockaddr_in address;
    char broadcast_msg[] = "Welcome to Phantom Ink!!!\n";
    char* board = createEmptyBoard();
    size_t boardPos;
    char writerMessage[] = "You have been assigned as the writer\n";
    int deckIndex = 0;
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
    assignQuestions(team_questions, &deckIndex);

    char word_msg[100];
    snprintf(word_msg, sizeof(word_msg), "The word is %s\n",secret_word);

    boardPos = 19; //Remove hardcoding the numbers
    int nextLinePos = 16;
    int newLinePos = 13;
    int lineOffset = 0;
    int guesserSocket;
    if(DEBUG != 1){
        broadcast(client_sockets, board);
        broadcastToWriters(teams, writerMessage);
        broadcastToWriters(teams, word_msg);
        while(boardPos < strlen(board)){
            for(int i = 0; i < MAX_TEAMS; i++){
                printf("\n>>> STARTING TURN: Team %d <<<\n", i);
                char turn_msg[100];
                snprintf(turn_msg, sizeof(turn_msg), "\n--- TEAM %d: YOUR TURN ---\n", i + 1);
                broadcastToGuessers(teams, i, turn_msg);
                send(teams[i][0], turn_msg, strlen(turn_msg), 0);
                for(int j = 0; j < MAX_QUESTIONS; j++){
                    char formatted_msg[2048]; 
                    snprintf(formatted_msg, sizeof(formatted_msg), "%d) %s\n", j + 1, team_questions[i][j]);
                    printf("Broadcast questions to guessers Team: %d\n", i);
                    broadcastToGuessers(teams,i,formatted_msg);
                }
                char choice = listenToGuessers(teams,i,team_questions,false,&guesserSocket);
                size_t wordIndex = 0;
                if(choice == GUESSER_GUESS_TURN){
                    printf("Team %d chose to guess the word\n", i);
                    char msg[100];
                    while(true){
                        char letter = listenToWriter(guesserSocket,i,true);
                        boardPos += 1;
                        lineOffset += 1;
                        board[boardPos] = letter;
                        broadcast(client_sockets,board);
                        if(letter == secret_word[wordIndex]){
                            printf("Team %d guessed the correct letter: %c\n", i, letter);
                            wordIndex += 1;
                            snprintf(msg, sizeof(msg), "Team %d guessed the correct letter.\n", i + 1);
                            broadcast(client_sockets, msg);
                        }else{
                            snprintf(msg, sizeof(msg), "Team %d guessed the wrong letter.\n", i + 1);
                            broadcast(client_sockets, msg);
                            if(i == MAX_TEAMS - 1){
                                boardPos += (newLinePos - lineOffset);
                            }else{
                                boardPos += (nextLinePos - lineOffset);
                            }
                            lineOffset = 0;
                            break;
                        }
                        if(wordIndex == strlen(secret_word)){
                            printf("Team %d guessed the word and wins the game!!!\n", i + 1);
                            closeClients(client_sockets, i + 1);
                            freeTeams(teams);
                            free(board);
                            printf("\nSession complete. Shutting down server.\n");
                            close(server_fd);
                            return 0;
                        }
                    }
                    continue;
                }
                // It's expected for choice to be the char version of a number
                int index = rand() % (LENGTH_QUESTIONS - deckIndex);
                int questionIndex = choice - '0';
                questionIndex -= 1;
                team_questions[i][questionIndex] = PHANTOM_QUESTIONS[index];
                char* temp = PHANTOM_QUESTIONS[index];
                PHANTOM_QUESTIONS[index] = PHANTOM_QUESTIONS[LENGTH_QUESTIONS - deckIndex - 1];
                PHANTOM_QUESTIONS[LENGTH_QUESTIONS - deckIndex - 1] = temp;
                printf("Team %d Has Question %d filled: %s\n", i, questionIndex + 1, team_questions[i][questionIndex]);
                deckIndex += 1;
                deckIndex %= LENGTH_QUESTIONS;
                while(choice != GUESSER_END_TURN){
                    char letter = listenToWriter(teams[i][0],i,false);
                    boardPos += 1;
                    lineOffset += 1;
                    board[boardPos] = letter;
                    broadcast(client_sockets,board);
                    if(letter == WRITER_END_TURN){
                        break;
                    }
                    choice = listenToGuessers(teams,i,team_questions,true,&guesserSocket);
                }
                // Don't forget about case where word is longer than board. (Can do later)
                // Implement the Sun spots (Can do later)
                

                //Move to next team line
                if(i == MAX_TEAMS - 1){
                    boardPos += (newLinePos - lineOffset);
                }else{
                    boardPos += (nextLinePos - lineOffset);
                }
                lineOffset = 0;
            }
        }
    }

    printf("%s",board);

    closeClients(client_sockets, -1);

    freeTeams(teams);
    free(board);

    printf("\nSession complete. Shutting down server.\n");
    close(server_fd);

    return 0;
}

void assignQuestions(char* team_questions[MAX_TEAMS][MAX_QUESTIONS], int* deckIndex){
    int index;
    char* temp;
    for(int i = 0; i < MAX_TEAMS; i++){
        for(int j = 0; j < MAX_QUESTIONS; j++){
            index = rand() % (LENGTH_QUESTIONS - *deckIndex);
            team_questions[i][j] = PHANTOM_QUESTIONS[index];
            temp = PHANTOM_QUESTIONS[index];
            PHANTOM_QUESTIONS[index] = PHANTOM_QUESTIONS[LENGTH_QUESTIONS - *deckIndex - 1];
            PHANTOM_QUESTIONS[LENGTH_QUESTIONS - *deckIndex - 1] = temp;
            printf("Team %d Question %d: %s\n", i, j + 1, team_questions[i][j]);
            *deckIndex += 1;
            *deckIndex %= LENGTH_QUESTIONS;
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

char listenToWriter(int sd, int teamNum, bool isGuesser) {
    char buffer[1024];
    char letter = '\0';
    char* prompt="Please enter a Letter: ";
    char* error;
    if(isGuesser){
        error = "Invalid! Send a single letter (A-Z): ";
    }else{
        error = "Invalid! Send a single letter (A-Z) or a period (.) to end the turn: ";
    }


    char trash[1024];
    // Keep reading until the buffer is empty
    while (recv(sd, trash, sizeof(trash), MSG_DONTWAIT) > 0);

    send(sd, prompt, strlen(prompt), 0);
    while (letter < 'A' || letter > 'Z') {
        memset(buffer, 0, sizeof(buffer));
        
        int valread = recv(sd, buffer, sizeof(buffer) - 1, 0);
        
        if (valread != 2){
            send(sd, error, strlen(error), 0);
            continue;
        }

        letter = buffer[0];

        if (letter >= 'a' && letter <= 'z') {
            letter -= 32; 
        }

        if ((letter >= 'A' && letter <= 'Z') || (!isGuesser && letter == WRITER_END_TURN)) {
            printf("Team %d Writer sent letter: %c\n", teamNum, letter);
            char msg[64];
            snprintf(msg, sizeof(msg), "The Spirit writes: %c\n", letter);
            printf("%s",msg);
            return letter;
        } else {
            send(sd, error, strlen(error), 0);
        }
    }
    return '0';
}

char listenToGuessers(int** teams, int teamNum, char* team_questions[MAX_TEAMS][MAX_QUESTIONS], bool askedQuestion, int* guesserSocket) {
    char buffer[1024];
    int choice = -1;
    int num_per_team = MAX_CLIENTS / MAX_TEAMS;
    int leftover = MAX_CLIENTS % MAX_TEAMS;
    int current_team_size = num_per_team + (teamNum < leftover ? 1 : 0);
    char error[100];
    char prompt[100];
    char trash[1024];
    if(askedQuestion){
        snprintf(error, sizeof(error), "Invalid choice! Please enter the letter %c or the letter %c: ", GUESSER_END_TURN, GUESSER_CONTINUE_TURN);
        snprintf(prompt, sizeof(prompt), "Please End Turn (%c) or Continue Word (%c): ", GUESSER_END_TURN, GUESSER_CONTINUE_TURN);
    }else{
        snprintf(error, sizeof(error), "Invalid choice! Please enter a number between (1-%d) or guess the word (%c): ", MAX_QUESTIONS, GUESSER_GUESS_TURN);
        snprintf(prompt, sizeof(prompt), "Please Select a Question (1-%d) or guess the word (%c): ", MAX_QUESTIONS, GUESSER_GUESS_TURN);
    }

    // Flush existing data in the buffers so players can't "pre-type"
    for (int i = 1; i < current_team_size; i++) {
        // Keep reading until the buffer is empty
        while (recv(teams[teamNum][i], trash, sizeof(trash), MSG_DONTWAIT) > 0);
    }

    broadcastToGuessers(teams,teamNum,prompt);
    // Loop until we get a valid choice (0-4 because of choice -= 1)
    while (choice < 0 || choice >= MAX_QUESTIONS) {
        fd_set readfds;
        FD_ZERO(&readfds);
        int max_sd = 0;

        for (int i = 1; i < current_team_size; i++) {
            int sd = teams[teamNum][i];
            FD_SET(sd, &readfds);
            if (sd > max_sd) max_sd = sd;
        }

        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0) { continue; }

        for (int i = 1; i < current_team_size; i++) {
            int sd = teams[teamNum][i];
            *guesserSocket = sd;
            if (FD_ISSET(sd, &readfds)) {
                int valread = recv(sd, buffer, sizeof(buffer) - 1, 0);

                if (valread != 2){
                    send(sd, error, strlen(error), 0);
                    continue;
                }

                buffer[valread] = '\0';
                
                if (buffer[0] >= 'a' && buffer[0] <= 'z') {
                    buffer[0] -= 32; 
                }

                if (!askedQuestion && buffer[0] >= '1' && buffer[0] <= (MAX_QUESTIONS + '0')) {
                    int input_val = atoi(buffer); 
                    choice = input_val - 1; 
                    char msg[100];
                    printf("Team %d chose question: %s\n", teamNum, team_questions[teamNum][choice]);
                    snprintf(msg, sizeof(msg), "%s\n", team_questions[teamNum][choice]);
                    send(teams[teamNum][0], msg, strlen(msg), 0);
                } else if(!askedQuestion && buffer[0] == GUESSER_GUESS_TURN){
                     printf("Team %d chose to end turn\n", teamNum);
                     char msg[100];
                     snprintf(msg, sizeof(msg), "Your teammate is going to guess the word.\n");
                     send(teams[teamNum][0], msg, strlen(msg), 0);
                } else if(askedQuestion && buffer[0] == GUESSER_CONTINUE_TURN){
                    printf("Team %d chose to ask for another letter\n", teamNum);
                    char msg[100];
                    snprintf(msg, sizeof(msg), "Your teammate(s) have asked for another letter.\n");
                    send(teams[teamNum][0], msg, strlen(msg), 0);
                } else if(askedQuestion && buffer[0] == GUESSER_END_TURN){
                    printf("Team %d chose to end turn\n", teamNum);
                    char msg[100];
                    snprintf(msg, sizeof(msg), "Your teammate(s) have ended the turn.\n");
                    send(teams[teamNum][0], msg, strlen(msg), 0);
                } else {
                    send(sd, error, strlen(error), 0);
                    continue;
                }
                return buffer[0];
            }
        }
    }
    return '0';
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

void closeClients(int client_sockets[], int teamNum){
    if(teamNum > 0){
        char msg[100];
        snprintf(msg, sizeof(msg), "Team %d has won the game!!!\n", teamNum + 1);
        broadcast(client_sockets, msg);
    }
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

