#include "gamelogic.h"

// nc 0.0.0.0 8080

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

    struct sockaddr_in name;
    socklen_t namelen = sizeof(name);
    getsockname(server_fd, (struct sockaddr *)&name, &namelen);
    printf("Server listening on %s:%d\n", inet_ntoa(name.sin_addr), ntohs(name.sin_port));
    printf("Waiting for %d clients to connect...\n", MAX_CLIENTS);

    if(DEBUG != 1){
        collectClients(server_fd, address, client_sockets);
    }

    printf("\nAll clients connected. Sending broadcast message...\n");
    if(DEBUG != 1){
        broadcast(client_sockets, broadcast_msg);
    }

    assignTeams(client_sockets, teams);
    assignWords(&secret_word);
    assignQuestions(team_questions, &deckIndex);

    char word_msg[100];
    snprintf(word_msg, sizeof(word_msg), "The word is %s\n",secret_word);

    boardPos = 19; 
    int nextLinePos = 16;
    int newLinePos = 13;
    int lineOffset = 0;
    int guesserSocket;
    int loopCounter = 0;

    if(DEBUG != 1){
        broadcastToWriters(teams, writerMessage);
        broadcastToWriters(teams, word_msg);
        while(loopCounter < 8){
            for(int i = 0; i < MAX_TEAMS; i++){
                printf("\n>>> STARTING TURN: Team %d <<<\n", i);
                char turn_msg[100];
                snprintf(turn_msg, sizeof(turn_msg), "\n--- TEAM %d: YOUR TURN ---\n", i + 1);
                broadcastToGuessers(teams, i, turn_msg);
                send(teams[i][0], turn_msg, strlen(turn_msg), 0);
                for(int j = 0; j < MAX_QUESTIONS; j++){
                    char formatted_msg[2048]; 
                    snprintf(formatted_msg, sizeof(formatted_msg), "%d) %s\n", j + 1, team_questions[i][j]);
                    broadcastToGuessers(teams,i,formatted_msg);
                }
                char choice;
                if (loopCounter == 7) {
                    char final_msg[] = "This is the final turn! You can only guess the word.\n";
                    broadcastToGuessers(teams, i, final_msg);
                    choice = GUESSER_GUESS_TURN;
                    guesserSocket = teams[i][1]; 
                }else{
                    choice = listenToGuessers(teams,i,team_questions,false,&guesserSocket);
                }
                size_t wordIndex = 0;
                if(choice == GUESSER_GUESS_TURN){
                    printf("Team %d chose to guess the word\n", i);
                    char msg[100];
                    while(true){
                        char letter = listenToWriter(guesserSocket,true);
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
                            cleanUp(server_fd, client_sockets, teams, board);
                            return 0;
                        }
                    }
                    continue;
                }
                int index = rand() % (LENGTH_QUESTIONS - deckIndex);
                int questionIndex = choice - '0';
                questionIndex -= 1;
                team_questions[i][questionIndex] = PHANTOM_QUESTIONS[index];
                char* temp = PHANTOM_QUESTIONS[index];
                PHANTOM_QUESTIONS[index] = PHANTOM_QUESTIONS[LENGTH_QUESTIONS - deckIndex - 1];
                PHANTOM_QUESTIONS[LENGTH_QUESTIONS - deckIndex - 1] = temp;
                deckIndex += 1;
                deckIndex %= LENGTH_QUESTIONS;
                while(choice != GUESSER_END_TURN){
                    char letter = listenToWriter(teams[i][0],false);
                    boardPos += 1;
                    lineOffset += 1;
                    board[boardPos] = letter;
                    broadcast(client_sockets,board);
                    if(letter == WRITER_END_TURN){
                        break;
                    }
                    choice = listenToGuessers(teams,i,team_questions,true,&guesserSocket);
                }

                if(i == MAX_TEAMS - 1){
                    boardPos += (newLinePos - lineOffset);
                }else{
                    boardPos += (nextLinePos - lineOffset);
                }
                lineOffset = 0;
            }
            loopCounter += 1;
        }
    }
    
    cleanUp(server_fd, client_sockets, teams, board);

    return -1;
}