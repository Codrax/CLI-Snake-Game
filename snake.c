#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <time.h>
#include <math.h>

#define VERSION "1.0.0"

// Basic colored logging macros
#define LOG_ERROR(msg, ...)   fprintf(stderr, "\x1b[31m[ERROR]\x1b[0m " msg "\n", ##__VA_ARGS__)    // Red
#define LOG_WARNING(msg, ...) fprintf(stderr, "\x1b[33m[WARNING]\x1b[0m " msg "\n", ##__VA_ARGS__)  // Yellow
#define LOG_SUCCESS(msg, ...) fprintf(stdout, "\x1b[32m[SUCCESS]\x1b[0m " msg "\n", ##__VA_ARGS__) // Green
#define LOG_INFO(msg, ...)    fprintf(stdout, "\x1b[36m[INFO]\x1b[0m " msg "\n", ##__VA_ARGS__)    // Cyan/blue
#define LOG_STATUS(msg, ...)  fprintf(stdout, "\x1b[34m[STATUS]\x1b[0m " msg "\n", ##__VA_ARGS__)  // Blue

// Color
#define C_RED     "\x1b[31m"
#define C_YELLOW  "\x1b[33m"
#define C_BLUE    "\x1b[34m"
#define C_LIME    "\x1b[92m"
#define RESET   "\x1b[0m"

// Cursor ctl.
#define LINE printf("\n")
#define LINE_START "\r"
#define LINE_UP "\033[A"
#define LINE_UP_N(n) printf("\033[%dA", n)

#define CURSOR_COL(c) printf("\033[%dG", c)
#define CURSOR_ROW(r) printf("\033[%dd", r)
#define CURSOR_POS(r,c) printf("\033[%d;%dH", r, c)

#define SCROLL_UP(n) printf("\033[%dS", n)
#define CLEAR_SCREEN() printf("\033[2J\033[H")

#define CURSOR_HIDE() printf("\033[?25l")
#define CURSOR_SHOW() printf("\033[?25h")

// Characters
#define CHAR_SNAKE "🐍"

char* CHAR_SNAKE_HEAD = "🐍";
char* CHAR_SNAKE_BODY = "🟩";
char* CHAR_SNAKE_TAIL = "🟢";

char* CHAR_APPLE = "🍎";
char* CHAR_WALL = "🧱";
int CHAR_WALL_SIZE = 2;

char* CHAR_EMPTY = "　";
int CHAR_EMPTY_SIZE = 2;

#define TABLE_YSTART 2 + 2
// TABLE_XSTAR

// TERMINAL CTL
struct termios orig_termios;

void disableRawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    CURSOR_SHOW();
}

void enableRawMode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disableRawMode);

    struct termios raw = orig_termios;

    /* input modes */
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

    /* output modes */
    raw.c_oflag &= ~(OPOST);

    /* control modes */
    raw.c_cflag |= (CS8);

    /* local modes */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

    /* control characters */
    raw.c_cc[VMIN] = 0;   // read returns after 1 byte
    raw.c_cc[VTIME] = 0;  // no timeout

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int readKey() {
    char c;
    if (read(STDIN_FILENO, &c, 1) == 1)
        return c;
    return -1;
}

// Custom structs
typedef struct {
	int x;
	int y;
} t_point;

// Internal
int** table; // a long ass double matrix
int gameScore=0;
t_point* listSnake=NULL;
int listSnakeCount = 0;
t_point* listApple=NULL;
int listAppleCount = 0;

int snakeDirection=0; // 0 = up, 1 = left, 2 = bottom, 3 = right

// Settings
int tableX=20; // size X
int tableY=20; // size Y
int tableResolution=1; // print size

unsigned int snakeMoveDelay=300; // 0 to disable
unsigned int appleAddDelay=10000; // 0 to disable

bool addNewAppleOnEat=true;
bool addInitialApple=true;
bool teleportingWalls=false;

// Utils
char* generateStrLen(char* source, int length) {
	int blockSize = strlen(source);
	char* s = malloc((length*blockSize+1) * sizeof(char));
	for (int i=0; i<length; i++)
		memcpy(&s[i*blockSize], source, blockSize);
	s[length*blockSize] = 0;

	return s;
}

int pointIndexInList(t_point *point, t_point* list, int listCount) {
	while (listCount > 0) {
		// Next (and index)
		listCount--;

		// Is it this one?
		if (list[listCount].x == point->x && list[listCount].y == point->y)
			return listCount;
	}

	return -1;
}

bool pointInList(t_point *point, t_point* list, int listCount) {
	while (listCount > 0) {
		// Next (and index)
		listCount--;

		// Is it this one?
		if (list[listCount].x == point->x && list[listCount].y == point->y)
			return true;
	}

	return false;
}

void deleteFromList(t_point* list, int *listCount, int removeIndex) {
	for (int i=removeIndex; i<*listCount-1; i++)
		list[i] = list[i+1];

	--*listCount;
}

double randomFloat() {
	return (double)rand() / RAND_MAX;
}
int randomRange(int from, int to) {
	return from + randomFloat() * (to-from);
}

// Drawing
void initializeBoard() {
	char* s = generateStrLen(CHAR_EMPTY, (tableX+2)*tableResolution);
	for (int i=0; i<(tableY+2)*tableResolution; i++) {
		CURSOR_POS(TABLE_YSTART + i, 0);
		printf("%s", s);
	}
	
	// Free
	free(s);	
}

int computeX(int X) {
	int v = 0;

	// wall
	if (X >= 0)
		v += tableResolution*CHAR_WALL_SIZE;

	if (X > 0)
		v += X*tableResolution*CHAR_EMPTY_SIZE;

	return v+1;
}

int computeY(int Y) {
	return TABLE_YSTART + 1 + (Y+1)*tableResolution;
}

void drawWall() {
	int Y, i;
	
	// Top and bottom
	char *walliShat = generateStrLen(CHAR_WALL, (tableX+2)*tableResolution);
	for (i=0; i<tableResolution; i++) {
		CURSOR_POS(computeY(-1)+i, 0);
		printf("%s", walliShat);
	}
	for (i=0; i<tableResolution; i++) {
		CURSOR_POS(computeY(tableY)+i, 0);
		printf("%s", walliShat);
	}

	// Reduce the wally waller to 1x1
	walliShat[strlen(CHAR_WALL)*tableResolution] = 0;

	// Left and right
	for (Y=0; Y<tableY; Y++) {
		// Left
		for (i=0; i<tableResolution; i++) {
			CURSOR_POS(computeY(Y)+i, 0);
			printf("%s", walliShat);
		}
		// Right
		for (i=0; i<tableResolution; i++) {
			CURSOR_POS(
				computeY(Y)+i,
				computeX(tableX)
				);
			printf("%s", walliShat);
		}
	}

	// Free
	free(walliShat);
}
void drawCellEx(t_point *pt, char* str) {
	// Compute type
	char* draw=generateStrLen(str, tableResolution);

	// Draw
	for (int i=0; i<tableResolution; i++) {
		CURSOR_POS(
			computeY(pt->y)+i,
			computeX(pt->x)
			);
		printf("%s", draw);
	}
}
void drawCell(t_point *pt) {
	if (pt->x < 0 || pt->y < 0 || pt->x >= tableX || pt->y >= tableY)
		return;

	// Prep
	char *chr=CHAR_EMPTY;
	int i;

	// Is it apple?
	if (pointInList(pt, listApple, listAppleCount))
		chr = CHAR_APPLE;

	// Is it snake?
	i = pointIndexInList(pt, listSnake, listSnakeCount);
	if (i != -1) {
		if (i == 0)
			chr = CHAR_SNAKE_HEAD;
		else
		if (i == listSnakeCount-1)
			chr = CHAR_SNAKE_TAIL;
		else
		chr = CHAR_SNAKE_BODY;
	}

	// Draw
	drawCellEx(pt, chr);
}
void drawScore() {
	CURSOR_POS(computeY(tableY+1)+1, 0);
	
	printf(C_LIME "SCORE: " RESET "%d" "\n", gameScore);
}
void drawCommand() {
	CURSOR_POS(computeY(tableY+1)+4, 0);
	printf(" >> ");
	CURSOR_POS(computeY(tableY+1)+4, 5);
}
void drawSnake() {
	for (int i=0; i<listSnakeCount; i++)
		drawCell( &listSnake[i] );
}
void fullUpdateTable() {
	t_point pt;
	for (pt.y=0; pt.y<tableY; pt.y++)
		for (pt.x=0; pt.x<tableX; pt.x++)
			drawCell(&pt);
}
void fullUpdate() {
	drawWall();
	fullUpdateTable();
}

bool hasAvailableSpace() {
	return listAppleCount+listSnakeCount < tableX * tableY;
}

bool isSpaceFilleBySnake() {
	return listSnakeCount >= tableX * tableY;
}

// Gameplay
bool gameAddApple() {
	// Has enough space?
	if (!hasAvailableSpace())
		return false;

	// Create
	t_point coord;
	do {
		coord.x = randomRange(0, tableX);
		coord.y = randomRange(0, tableY);
	} while (pointInList(&coord, listApple, listAppleCount) 
			|| pointInList(&coord, listSnake, listSnakeCount));

	// Add
	listAppleCount++;
	listApple = realloc(listApple, listAppleCount * sizeof(t_point));
	if (!listApple) {
		//free(listApple);
		free(listSnake);
		
		perror("SEVERE ERROR");
		exit(0x0F);
	}
	listApple[listAppleCount-1] = coord;

	// Draw
	drawCell(&coord);
	//
	return true;
}

void printCenterText(char* text) {
	int Y = computeY(tableY / 2);
	int X = computeX(tableX / 2) - round(strlen(text) / 2);
	CURSOR_POS(Y, X);
	printf(text);
}

bool doMovement() {
	// Check new coordinate
	t_point new = listSnake[0];
	switch (snakeDirection) {
		case 0: new.y--; break;
		case 1: new.x++; break;
		case 2: new.y++; break;
		case 3: new.x--; break;
	}
	
	// Walls
		int hitWalls = new.x < 0 || new.y < 0
				|| new.x >= tableX || new.y >= tableY;
		if (hitWalls && teleportingWalls) {
			// Teleport X	
			if (new.x == -1)
				new.x = tableX-1;
			if (new.x == tableX)
				new.x = 0;
			if (new.y == -1)
				new.y = tableY-1;
			if (new.y == tableY)
				new.y = 0;

			hitWalls = 0;
		}

	// Apple
		int i = pointIndexInList(&new, listApple, listAppleCount);
	// Self
		int j = pointIndexInList(&new, listSnake, listSnakeCount);

	// Hit APPLE
	if (i != -1) {
		deleteFromList(listApple, &listAppleCount, i);
		printf("Got apple!                     ");

		// Re-alloc
		listSnakeCount++;
		listSnake = realloc(listSnake, sizeof(t_point)*listSnakeCount);
		if (listSnake == NULL) {
			free(listApple);
			
			perror("SEVERE ERROR");
			exit(0x0F);
		}

		// Offset
		for (i=listSnakeCount-1; i>0; i--)
			listSnake[i] = listSnake[i-1];

		// Set head
		listSnake[0] = new;

		// Draw needed
		drawCell(&listSnake[0]);
		drawCell(&listSnake[1]);
		drawCell(&listSnake[listSnakeCount-1]);

		// Score
		gameScore++;

		// New apple?
		if (addNewAppleOnEat)
			gameAddApple();
	} else
	// Hit WALL or SELF?
	if ( (j != -1 && j != listSnakeCount-1) // exclude tail
		|| hitWalls) {
		printf("You died!!                      ");

		// Print
		printCenterText("GAME OVER");

		return false;
	} else
	// Hit NONE - DEFAULT (move)
	{
		t_point oldTail = listSnake[listSnakeCount-1];

		// Offset entire thingie
		for (i=listSnakeCount-1; i>0; i--)
			listSnake[i] = listSnake[i-1];

		// Set new head position
		listSnake[0] = new;	
		
		// Draw snake
		drawCell(&oldTail);
		drawCell(&listSnake[0]);
		drawCell(&listSnake[1]);
		drawCell(&listSnake[listSnakeCount-1]);
	}

	// Update score
	drawScore();
	return true;
}

// PARAM
int g_argc;
char** g_args;

int param_get_index(char* name) {
	char** aEnum=g_args;
	int index=0;
	while (*aEnum) {
		if (strcmp(*aEnum, name) == 0)
			return index;
		index++;
		aEnum++;
	}
	return -1;
}
bool param_exists(char* name) {
	char** aEnum=g_args;
	while (*aEnum) {
		if (strcmp(*aEnum, name) == 0)
			return 1;
		aEnum++;
	}
	return 0;
}
char* param_get_value(char* name) {
	char** aEnum=g_args;
	while (*aEnum) {
		if (strcmp(*aEnum, name) == 0) {
			if (*(aEnum+1))
				return *(aEnum+1);
			return NULL;
		}
		aEnum++;
	}
	return NULL;
}

// STR ARRAY
bool array_str_contains(char** array, int size, char* subject) {
	for (int i=0; i<size; i++)
		if (strcmp(subject, array[i]) == 0)
			return true;
	return false;
}
bool narray_str_contains(char** array, char* subject) {
	while (*array) {
		if (strcmp(subject, *array) == 0)
			return true;
		array++;
	}
	return false;
}

// STR
void str_toupper(char* subject) {
	while (*subject) {
		*subject = toupper((unsigned char)*subject);
		subject++;
	}
}
void str_ntoupper(char* subject, int n) {
	while (*subject && n > 0) {
		*subject = toupper((unsigned char)*subject);
		subject++;
		n--;
	}
}

void str_tolower(char* subject) {
	while (*subject) {
		*subject = tolower((unsigned char)*subject);
		subject++;
	}
}
void str_ntolower(char* subject, int n) {
	while (*subject && n > 0) {
		*subject = tolower((unsigned char)*subject);
		subject++;
		n--;
	}
}

bool str_contains_char(char* haystack, char needle) {
	while (*haystack) {
		if (*haystack == needle) return true;
		haystack++;
	}
	return false;
}

bool str_contains(char* haystack, char* needle) {
	return strstr(haystack, needle) != NULL;
}

bool str_starts_with(const char* haystack, const char* needle) {
    while (*needle) {
        if (*haystack != *needle) return false;
        haystack++;
        needle++;
    }
    return true;
}

bool str_ends_with(const char* haystack, const char* needle) {
    size_t hlen = strlen(haystack);
    size_t nlen = strlen(needle);
    if (nlen > hlen) return false;      // needle longer than haystack → can't match
    return strcmp(haystack + hlen - nlen, needle) == 0;
}

// CHAR
bool char_in_set(char needle, char* haystack) {
	while (*haystack) {
		if (*haystack == needle) return true;
		haystack++;
	}
	return false;
}

// STR
#define TRIM_SYMBOLS "\n\t" // cannot contain \0 :P

void str_trim_left(char* subject) {
    if (!subject) return;

    char* start = subject;

    while (*start && char_in_set(*start, TRIM_SYMBOLS))
        start++;

    if (start != subject)
        memmove(subject, start, strlen(start) + 1);
}

void str_trim_right(char* subject) {
	if (!*subject) return; // empty ahh
	char* reference = subject;
	subject += strlen(subject)-1;
	do {
		if (char_in_set(*subject, TRIM_SYMBOLS))
			*subject = '\0';
		else
			break;
		subject--;
	} while (subject >= reference);
}

// APP
void print_header() {
	printf("Snake\n");
	printf("----------------------\n");
	printf("Copyright (c) Codrut Software\n");
}
void print_usage() {
	printf("Usage:\n");
	printf(" --help, -h\n");
	printf("    display help information and commands\n");
	printf(" -version, -v\n");
	printf("    show version information\n");
	printf("\n");
	printf(" --infinity\n");
	printf("    infinity game mode\n");
	printf("\n");
	printf(" --no-initial-apple\n");
	printf("    do not add apple on game start\n");
	printf(" --no-apple-on-eat\n");
	printf("    do not add apple on eat\n");
	printf("\n");
	printf(" --sizeX <int> \n");
	printf("    Size of table\n");
	printf(" --sizeY <int> \n");
	printf("    Size of table\n");
	printf(" --resolution <int> \n");
	printf("    Resolution of each cell\n");
	printf("\n");
	printf(" --time-apple <int> \n");
	printf("    Milliseconds betwen each apple add (0-disabled)\n");
	printf(" --time-move <int> \n");
	printf("    Milliseconds betwen each movement (0-disabled)\n");
	
}
void print_help() {
	print_header();
	printf("\n");
	print_usage();
}
void print_version() {
	print_header();
	printf("\n");
	printf("Version "VERSION"\n");
}

bool ask_confirm(char* prompt, bool defaultToNone, bool defaultToInvalid) {
	printf("\x1b[1;34m::\x1b[0m %s [%c/%c] ", prompt, 
		defaultToNone ? 'Y' : 'y', 
		defaultToNone ? 'n' : 'N'
		);
		
	size_t n;
	int char_cnt;
	char *result = NULL;
	char_cnt = getline(&result, &n, stdin);
	str_trim_right(result);
	char_cnt = strlen(result);
	
	switch (char_cnt) {
		//
		case 0: return defaultToNone;
		
		// Y/N
		case 1: {
			if (*result == 'y' || *result == 'Y') {
				free(result);
				return true;
			}
			if (*result == 'n' || *result == 'N') {
				free(result);
				return false;
			}
		} break;

		// NO
		case 2: {
			str_toupper(result);
			if (strcmp(result, "NO") == 0) {
				free(result);
				return false;
			}
		} break;

		// YES
		case 3: {
			str_toupper(result);
			printf("Read '%s'", result);
			if (strcmp(result, "YES") == 0) {
				free(result);
				return true;
			}
		}
	}
	free(result);
	return defaultToInvalid;
}
bool ask_confirm_standard(char* prompt, bool defaultToNone) {
	return ask_confirm(prompt, defaultToNone, false);
}

int main(int argc, char** argv) {
	g_argc = argc;
	g_args = argv;
	char* knownParameters[] = {
	   "--help",
	   "--version",
	   
	   "--infinity",
	   "--no-initial-apple",
	   "--no-apple-on-eat",

	   "--sizeX",
	   "--sizeY",
	   "--resolution",

	   "--time-apple",
	   "--time-move",
	   	   
	   "-h",
	   "-v",

	   NULL
	};

	// Santize parameters
	for (int i=1; i<argc; i++) {
		// Is param? --X \\ -X, non digit
		if (!str_starts_with(argv[i], "--") && !(strlen(argv[i]) >= 2 && argv[i][0] == '-' && !isdigit(argv[i][1])))
			continue;

		// Check
		if (!narray_str_contains(knownParameters, argv[i])) {
			LOG_ERROR("Unrecognized parameter \"%s\". Call with --help for usage", argv[i]);
			exit(0x02);
		}
	}

	// Information operation mode
	if (param_exists("--help") || param_exists("-h")) {
		print_help();
		exit(0);
	}
	if (param_exists("--version") || param_exists("-v")) {
		print_version();
		exit(0);
	}

	if (param_exists("--infinity")) {
		teleportingWalls = true;
		CHAR_WALL = "🌌";
	}
	if (param_exists("--no-initial-apple")) {
		addInitialApple=false;
	}
	if (param_exists("--no-apple-on-eat")) {
		addNewAppleOnEat = false;
	}

	char *s;
		
	s = param_get_value("--sizeX");
	if (s) {
		tableX = atol(s);
	}
	s = param_get_value("--sizeY");
	if (s) {
		tableY = atol(s);
	}
	s = param_get_value("--resolution");
	if (s) {
		tableResolution = atol(s);
	}
	s = param_get_value("--time-apple");
	if (s) {
		appleAddDelay = atol(s);
	}
	s = param_get_value("--time-move");
	if (s) {
		snakeMoveDelay = atol(s);
	}
	

	printf("Starting up snake game...");	

	// Initialize enviroment
	CURSOR_HIDE();
	enableRawMode();
	srand(time(NULL));
	
	// Prepare for drawing
	SCROLL_UP(10);
	CLEAR_SCREEN();
	printf(CHAR_SNAKE "          SNAKE GAME           " CHAR_SNAKE "\n\r");
	printf(          "-=================================-\n\r");
	printf("(c) 2026 Codrut Software\n\r");
	LINE;

	// Initialize snake
	listSnakeCount = 2;
	listSnake = malloc(sizeof(t_point) * listSnakeCount);
	if (listSnake == NULL) {
		perror("SEVERE ERROR");
		exit(0x0F);
	}
	listSnake[0].x = floor(tableX / 2);
	listSnake[0].y = floor(tableY / 2);
	listSnake[1].x = listSnake[0].x;
	listSnake[1].y = listSnake[0].y;
	if (tableY/2+1 < tableY)
		listSnake[1].y++;
	else if (tableY/2-1 >= 0)
		listSnake[0].y--;
	else if (tableX/2+1 < tableX) {
		listSnake[1].x++;
		snakeDirection = 3;
	}
	else if (tableX/2-1 >= 0) {
		listSnake[0].x--;
		snakeDirection = 1;
	}
	else listSnakeCount=0;
	
	// Initialize first apple
	if (addInitialApple)
		gameAddApple();

	// Draw
	initializeBoard();
	fullUpdate();

	// Draw bottom info
	CURSOR_POS(computeY(tableY+1)+1, 0);
	printf("\n"); // SCORE LINE
	printf(C_BLUE "Controls:" RESET "WASD/Arrow - Move\tR - Redraw\tQ - Quit\n");
	printf("\n");
	printf("\n"); // COMMAND

	// Draw
	drawScore();

	// Process
	int key = 0;
	int moveNow = 0; // 0 - no, 1 - move, 2 - skip time
	long unsigned int tickPassed=0;
	while (1) {
		drawCommand();
		key = readKey();

		// has won?
		if (isSpaceFilleBySnake()) {
			printf("You have won!                                ");
			printCenterText("YOU WON!");
			break;
		}

		//
		if (key == 'q')
			break;
		else if (key == 'r')
			fullUpdate();
		else if ((key == 'w' || key == 65) && snakeDirection != 2) {
			snakeDirection = 0;
			moveNow=1;
			printf("top                                ");
		}
		else if ((key == 'a' || key == 68) && snakeDirection != 1) {
			snakeDirection = 3;
			moveNow=1;
			printf("left                               ");
		}
		else if ((key == 's' || key == 66) && snakeDirection != 0) {
			snakeDirection = 2;
			moveNow=1;
			printf("down                               ");
		}
		else if ((key == 'd' || key == 67) && snakeDirection != 3) {
			snakeDirection = 1;
			moveNow=1;
			printf("right                             ");
		}
		drawCommand();

		// Check movement
		if (moveNow == 1) {
			if (!doMovement()) break;
			
			moveNow = 2;
		}
		if (snakeMoveDelay != 0 && tickPassed % snakeMoveDelay == 0) {
			if (moveNow == 0)
				if (!doMovement()) break;

			moveNow=0;
		}

		// Check add new apple
		if (appleAddDelay != 0 && tickPassed % appleAddDelay == appleAddDelay-1) {
			gameAddApple();

			drawCommand();
			printf("New apple popped up!              ");
		}

		// Tick
		tickPassed++;

		// Sleep
		usleep(1000); // 1 millisecond
	}

	// Game done
	CURSOR_POS(computeY(tableY+1)+6, 0);

	// Done
	return 0;
}
