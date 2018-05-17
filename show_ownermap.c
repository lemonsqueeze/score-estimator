/* show_ownermap:
 * Compute ogs score estimator output from board state and dead stones.
 * 
 * Source derived from Pachi, so comes under same license.
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdarg.h>
#include <assert.h>

void die(char *str, ...)   {  va_list ap; va_start(ap, str); vfprintf(stderr, str, ap); va_end(ap); exit(1);  }
void fail(char *msg)  {  perror(msg);  exit(1);  }
void usage()          {  fprintf(stderr, "usage: show_ownermap file.game dead_stones\n");  exit(1);  }

int board[19 * 19];
int boardsize = 0;
int boardsize2 = 0;

#define S_NONE  0
#define S_BLACK 1
#define S_WHITE 2

int other_color(int color)
{
	switch (color) {
		case S_BLACK: return S_WHITE;
		case S_WHITE: return S_BLACK;
	}
	die("shouldn't happen");
	return 0;
}

#define coord_xy(x, y)  ((y) * boardsize + (x))

int str2coord(char *str)
{
	assert(*str >= 'A' && *str <= 'Z');
	char xc = tolower(str[0]);
	int y = (atoi(str + 1) - 1);
	int x = xc - 'a' - (xc > 'i');
	//printf("coord: '%s' = %i %i\n", str, x, y);
	return coord_xy(x, y);
}

void print_board(int *board)
{
	for (int y = boardsize - 1; y >= 0; y--) {
		for (int x = 0; x < boardsize; x++) {
			int c = coord_xy(x, y);
			const char chr[] = ".XO."; // empty, black, white, dame
			char ch = chr[board[c]];
			
			printf("%c ", ch);
		}
		printf("\n");
	}	
	printf("\n\n");
}

#define foreach_neighbor(coord, code)  do { \
	int _x = (coord) % boardsize;	\
	int _y = (coord) / boardsize;	\
	if (_y >= 1) { \
		int c = coord_xy(_x, _y - 1); \
		do { code } while(0);	      \
	}				      \
	if (_x >= 1) { \
		int c = coord_xy(_x - 1, _y); \
		do { code } while(0);	      \
	}				      \
	if (_x < boardsize - 1) { \
		int c = coord_xy(_x + 1, _y); \
		do { code } while(0);	      \
	}				      \
	if (_y < boardsize - 1) { \
		int c = coord_xy(_x, _y + 1); \
		do { code } while(0);	      \
	} \
} while(0)

void parse_board(FILE *f)
{
	for (int y = boardsize - 1; y >= 0; y--) {
		char line[256];
                if (!fgets(line, sizeof(line), f))  die("Premature EOF.\n");

		if (!strncmp("komi ", line, 5))     {  y++; continue;  }
		if (!strncmp("handicap ", line, 9)) {  y++; continue;  }

		if (strlen(line) != boardsize * 2 &&
		    strlen(line) != boardsize * 2 - 1)  die("Line not %d char long: '%s'\n", boardsize * 2 - 1, line);
		
                for (unsigned int i = 0; i < boardsize * 2; i++) {
			int c = y * boardsize + i/2;
                        switch (line[i]) {
				case '.': board[c] = S_NONE;  break;
				case 'X': board[c] = S_BLACK; break;
				case 'O': board[c] = S_WHITE; break;
				default : die("Invalid stone '%c'\n", line[i]);
                        }
                        i++;
                        if (line[i] && line[i] != ' ' && line[i] != '\n' && line[i] != ')')
                                die("No space after stone %i: '%c'\n", i/2 + 1, line[i]);		       
		}
	}
}

void read_game_file(FILE *f)
{
	char *line, buf[128];
	int n = sizeof(buf);
	while ((line = fgets(buf, n, f))) {
		if (*line == '#')  continue;
		if (!strncmp("boardsize ", line, 10)) {
			boardsize = atoi(line + 10);
			assert(boardsize >= 0);
			assert(boardsize <= 19);
			boardsize2 = boardsize * boardsize;
			parse_board(f);
		}
	}
}

int dead_stones[19 * 19] = { 0, };
int ndead_stones = 0;

void read_dead_stones(FILE *f)
{
	char *line, buf[128];
	int n = sizeof(buf);
	while ((line = fgets(buf, n, f))) {
		if (!strcmp("=\n", line))  continue;
		if (!strcmp("\n", line))   continue;

		if (!strncmp("= ", line, 2))  line += 2;
		while (*line && *line != '\n') {
			int c = str2coord(line);
			dead_stones[ndead_stones++] = c;
			assert(board[c] == S_BLACK || board[c] == S_WHITE);
			line += 2;
			if (isdigit(*line))  line++;
			if (*line == ' ')    line++;
		}
	}
}


/* Owner map: 0: undecided; 1: black; 2: white; 3: dame */

/* One flood-fill iteration; returns true if next iteration
 * is required. */
static bool
board_tromp_taylor_iter(int *ownermap)
{
	bool needs_update = false;

	for (int c = 0; c < boardsize2; c++) {
		if (board[c])  continue;       	
		/* foreach free point */

		/* Ignore occupied and already-dame positions. */
		if (ownermap[c] == 3)
			continue;
		/* Count neighbors. */
		int nei[4] = {0};
		foreach_neighbor(c, {
			nei[ownermap[c]]++;
		});
		/* If we have neighbors of both colors, or dame,
		 * we are dame too. */
		if ((nei[1] && nei[2]) || nei[3]) {
			ownermap[c] = 3;
			/* Speed up the propagation. */
			foreach_neighbor(c, {
				if (board[c] == S_NONE)
					ownermap[c] = 3;
			});
			needs_update = true;
			continue;
		}
		/* If we have neighbors of one color, we are owned
		 * by that color, too. */
		if (!ownermap[c] && (nei[1] || nei[2])) {
			int newowner = nei[1] ? 1 : 2;
			ownermap[c] = newowner;
			/* Speed up the propagation. */
			foreach_neighbor(c, {
				if (board[c] == S_NONE && !ownermap[c])
					ownermap[c] = newowner;
			});
			needs_update = true;
			continue;
		}
	}
	return needs_update;
}

/* Tromp-Taylor Counting */
float
board_official_score_and_dame(int *dame)
{
	/* A point P, not colored C, is said to reach C, if there is a path of
	 * (vertically or horizontally) adjacent points of P's color from P to
	 * a point of color C.
	 *
	 * A player's score is the number of points of her color, plus the
	 * number of empty points that reach only her color. */

	int ownermap[19 * 19];
	int s[4] = {0};
	const int o[4] = {0, 1, 2, 0};
	for (int c = 0; c < boardsize2; c++) {
		ownermap[c] = o[board[c]];
		s[board[c]]++;
	}

	/* Process dead stones. */
	for (int i = 0; i < ndead_stones; i++) {
		//foreach_in_group(board, dead_stones[i]) {
		int c = dead_stones[i];
		int color = board[c];
		ownermap[c] = o[other_color(color)];
		s[color]--; s[other_color(color)]++;
	}

	/* We need to special-case empty board. */
	if (!s[S_BLACK] && !s[S_WHITE])
		return 0.0;

	while (board_tromp_taylor_iter(ownermap))
		/* Flood-fill... */;

	print_board(ownermap);

	int scores[4] = { 0, };
	*dame = 0;
	for (int c = 0; c < boardsize2; c++) {
		assert(ownermap[c] != 0);
		if (ownermap[c] == 3) {  (*dame)++; continue;  }
		scores[ownermap[c]]++;
	}

	return 0.5 + scores[S_WHITE] - scores[S_BLACK];
}


int main(int argc, char **argv)
{
	argc--;  argv++;
	if (argc != 2)  usage();

	char *gamefile = *argv;
	argc--;  argv++;
	FILE *f = fopen(gamefile, "r");
	if (!f)  fail(gamefile);
	read_game_file(f);
	
	printf("boardsize %i\n", boardsize);
	print_board(board);

	char *deadstonesfile = *argv;
	argc--;  argv++;
	FILE *g = NULL;
	if (!strcmp(deadstonesfile, "-"))  g = stdin;
	else                               g = fopen(deadstonesfile, "r");
	if (!g)  fail(deadstonesfile);
	read_dead_stones(g);

	int dame = 0;
	float score = -board_official_score_and_dame(&dame);
	printf("Dames: %i\n", dame);
	printf("Score: %f\n", score);
}
