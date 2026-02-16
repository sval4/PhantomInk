#include "gamelogic.h"

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

void broadcast(int client_sockets[], char* broadcast_msg){
    for (int i = 0; i < MAX_CLIENTS; i++) {
        send(client_sockets[i], broadcast_msg, strlen(broadcast_msg), 0);
    }
}

void closeClients(int client_sockets[], int teamNum){
    char msg[100];
    if(teamNum > -1){ // Changed > 0 to > -1 to match logic (team index starts at 0)
        snprintf(msg, sizeof(msg), "Team %d has won the game!!!\n", teamNum + 1);
        broadcast(client_sockets, msg);
    }else{
        snprintf(msg, sizeof(msg), "The game has ended.\n");
        broadcast(client_sockets, msg);
    }
    for (int i = 0; i < MAX_CLIENTS; i++) {
        close(client_sockets[i]);
        printf("[Client %d] Disconnected.\n", i + 1);
    }
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
        char team_msg[100];
        snprintf(team_msg, sizeof(team_msg), "You have been assigned to Team %d\n", team_index + 1);
        send(temp_pool[c], team_msg, strlen(team_msg), 0);
    }
    printf("\n");
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

void broadcastToWriters(int** teams, char* broadcast_msg){
    for (int i = 0; i < MAX_TEAMS; i++) {
        send(teams[i][0], broadcast_msg, strlen(broadcast_msg), 0);
    }
}

void assignWords(char** secret_word){
    int index;
    index = rand() % LENGTH_WORDS;
    *secret_word = PHANTOM_WORDS[index];
    printf("Secret Word: %s\n", *secret_word);
    printf("\n");
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
            *deckIndex += 1;
            *deckIndex %= LENGTH_QUESTIONS;
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

    for (int i = 1; i < current_team_size; i++) {
        while (recv(teams[teamNum][i], trash, sizeof(trash), MSG_DONTWAIT) > 0);
    }

    broadcastToGuessers(teams,teamNum,prompt);
    
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
                if (buffer[0] >= 'a' && buffer[0] <= 'z') buffer[0] -= 32; 

                if (!askedQuestion && buffer[0] >= '1' && buffer[0] <= (MAX_QUESTIONS + '0')) {
                    int input_val = atoi(buffer); 
                    choice = input_val - 1; 
                    char msg[100];
                    snprintf(msg, sizeof(msg), "%s\n", team_questions[teamNum][choice]);
                    send(teams[teamNum][0], msg, strlen(msg), 0);
                } else if(!askedQuestion && buffer[0] == GUESSER_GUESS_TURN){
                      char msg[100];
                      snprintf(msg, sizeof(msg), "Your teammate is going to guess the word.\n");
                      send(teams[teamNum][0], msg, strlen(msg), 0);
                } else if(askedQuestion && buffer[0] == GUESSER_CONTINUE_TURN){
                    char msg[100];
                    snprintf(msg, sizeof(msg), "Your teammate(s) have asked for another letter.\n");
                    send(teams[teamNum][0], msg, strlen(msg), 0);
                } else if(askedQuestion && buffer[0] == GUESSER_END_TURN){
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

char listenToWriter(int sd, bool isGuesser) {
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
        if (letter >= 'a' && letter <= 'z') letter -= 32; 

        if ((letter >= 'A' && letter <= 'Z') || (!isGuesser && letter == WRITER_END_TURN)) {
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

void cleanUp(int server_fd, int client_sockets[], int** teams, char* board) {
    printf("%s",board);
    closeClients(client_sockets, -1);
    freeTeams(teams);
    free(board);
    printf("\nSession complete. Shutting down server.\n");
    close(server_fd);
}