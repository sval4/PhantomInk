# Phantom Ink (Terminal Edition)

Phantom Ink is a **multiplayer, terminal-based word-guessing party game** inspired by social deduction and collaborative clue games.  
Players connect over the network using simple command-line tools and play in teams, alternating between **writers** and **guessers** as they attempt to uncover a hidden word letter by letter.

The game is implemented as a **C socket server** that coordinates turns, manages teams, distributes questions, and maintains a shared ASCII game board that is broadcast to all players in real time.

## What is the Game?

Phantom Ink is a **team-based word deduction game** played entirely in the terminal.

**Each team has:**

- **1 Writer** — knows the secret word and responds to questions by writing letters  
- **1 Guesser** — does **not** know the word and must infer it through gameplay

Teams take turns selecting questions or choosing to guess the word.  
Writers respond by revealing letters on a shared board.  
Guessers must decide when they have enough information to guess the word.

**The first team to correctly guess the entire word wins immediately.**

All communication happens over **TCP sockets**, making it easy to play remotely using basic networking tools.

## How to Play the Game

### Server starts and waits for players
The server waits for a fixed number of clients to connect.  
Once all players are connected, the game begins automatically.

### Teams are assigned
Players are divided into teams.  
**Each team has:**  
- A **writer** (who knows the word)  
- A **guesser** (who does not)

### The secret word is assigned
Writers receive the secret word privately.  
Guessers never see the word directly.

### Gameplay loop
Teams take turns.  
On a team’s turn:  
- The guesser is shown a list of questions  
- The guesser may:  
  - **Select a question**, or  
  - **Attempt to guess the word** (especially on the final turn)  
- Writers respond by writing letters to the board  
- The shared board is broadcast to all players after each letter

### Guessing the word
Guessers guess the word one letter at a time.  
- If a wrong letter is guessed, the turn ends  
- If the full word is guessed correctly, the game ends immediately and that team wins

### Final turn
On the final round, teams may **only guess the word**.  
No additional questions are allowed.

## How to Run the Game

### 1. Expose the Server Using pinggy.io

Because this is a raw TCP server, you’ll need to expose your local port if players are connecting remotely.

Start a Pinggy tunnel (replace 8080 if needed):

```
ssh -p 443 -R0:localhost:8080 tcp@pinggy.io
```

Pinggy will output a public TCP address and port.  
**Share this address with all players.**

**Example:**  
`tcp://a.pinggy.io:12345`

### 2. Build and Run the Server

From the project root directory:

```
make run
```

The server will:  
- Bind to the configured port  
- Wait for all clients to connect  
- Automatically start the game once everyone is present

## How Players Connect (Netcat)

Players connect using `netcat` (`nc`).

### Local Play
If playing on the same machine or LAN:

```
nc 127.0.0.1 8080
```

### Remote Play (via Pinggy)
Using the Pinggy address provided by the host:

```
nc <pinggy-host> <pinggy-port>
```

**Example:**

```
nc a.pinggy.io 12345
```

Once connected:  
- Follow on-screen prompts  
- Do **not** close the terminal during gameplay  
- Input is read one character at a time

## Things That Still Need to Be Implemented

The following features are planned but not yet implemented:

- **Sun Spots**  
  Special board mechanics that block or alter letter placement  
  Adds strategic risk to writing letters

- **Turn Timer**  
  Enforces time limits for writers and guessers  
  Prevents stalling and keeps the game moving

- **Words Longer Than the Board**  
  Current board assumes a fixed size  
  Needs logic to handle wrapping or dynamic resizing for longer words

## Notes

This game is intentionally **minimalist and terminal-driven**.  
All state is managed server-side.  
Clients are “dumb terminals” that only send input and display output.  
Currently only supports gameplay with 2 teams.
