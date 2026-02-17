#ifndef GAME_DATA_H
#define GAME_DATA_H

#define LENGTH_WORDS 30
#define LENGTH_QUESTIONS 30
#define MAX_QUESTIONS 5
#define GUESSER_END_TURN 'E'
#define GUESSER_CONTINUE_TURN 'C'
#define WRITER_END_TURN '.'
#define GUESSER_GUESS_TURN 'G'

// 'extern' tells the compiler these live in another file
extern char* PHANTOM_WORDS[];
extern char* PHANTOM_QUESTIONS[];

#endif