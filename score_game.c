
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdarg.h>
#include <assert.h>
#include <getopt.h>

void usage()
{
        fprintf(stderr, "Usage: score_game                                      <estimator_out\n");
        fprintf(stderr, "       score_game -r chinese  -k komi -a handi         <estimator_out\n");
        fprintf(stderr, "       score_game -r japanese -k komi -b bcap -w wcap  <estimator_out\n\n");
        fprintf(stderr,
                "Options: \n"
                "  -k, --komi KOMI                   set komi. \n"
                "  -a, --handicap HANDICAP           set handicap. \n"
                "  -r, --rules RULESET               rules to use: (default chinese) \n"
                "                                    japanese|chinese \n"
		"  -b, --captures-black N            set number of prisoners black has. \n"
		"  -w, --captures-white N            set number of prisoners white has. \n"
		"\n"
		"Reads estimator output and scores game. \n"
		"Without arguments, score assumes chinese rules, 0.5 komi and no handicap. \n"
		"\n"
		"To score actual game, set rules with their respective requirements: \n"
		"    chinese  rules: komi and handicap. \n"
		"    japanese rules: komi and b/w captures. \n"
		);
}

static struct option longopts[] = {
        { "komi",              required_argument, 0, 'k' },
        { "handicap",          required_argument, 0, 'a' },
        { "rules",             required_argument, 0, 'r' },
        { "captures-black",    required_argument, 0, 'b' },
        { "captures-white",    required_argument, 0, 'w' },
        { "help",              no_argument,       0, 'h' },
        { 0, 0, 0, 0 }
};

void fail(char *msg)  {  perror(msg);  exit(1);  }
void die(const char *format, ...)
{
        va_list ap;
	fprintf(stderr, "score_game: ");
        va_start(ap, format);  vfprintf(stderr, format, ap);  va_end(ap);
        exit(1);
}

enum go_ruleset {
        RULES_CHINESE, /* default */
        RULES_JAPANESE,
};

const char*
rules2str(enum go_ruleset rules)
{
        switch (rules) {
		case RULES_CHINESE:     return "chinese";
		case RULES_JAPANESE:    return "japanese";
		default:                die("unknown ruleset '%i'", rules);
        }
        return NULL;
}

enum go_ruleset
str2rules(char *str)
{
        if (!strcasecmp(str, "chinese"))        return RULES_CHINESE;
	if (!strcasecmp(str, "japanese"))       return RULES_JAPANESE;
	die("unknown rules: '%s'", str);  return 0;
}

enum go_ruleset rules = RULES_CHINESE;
int board[19 * 19];
int boardsize = 0;
int boardsize2 = 0;
float komi = 0.5;
int handicap = 0;
int captures[4] = { 0, };


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
			const char chr[] = " o_ "; // empty, black, white, offboard
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


void read_board(FILE *f, int *board)
{
	assert(boardsize);
	char *line, buf[128];
	int n = sizeof(buf);
	int y = boardsize - 1;
	while (y >= 0) {
		line = fgets(buf, n, f);
		int len = strlen(line);
		if (len != boardsize * 2 + 1)
			die("board line %i is not %i char long:\n%s\n", boardsize - y, boardsize * 2 + 1, line);

		assert(y >= 0);
		for (int x = 0; x < boardsize; x++) {
			int c = y * boardsize + x;
			switch (line[x * 2]) {
				case ' ':  board[c] = S_NONE;  break;
				case 'o':  board[c] = S_BLACK; break;
				case '_':  board[c] = S_WHITE; break;
				default:  die("shouldn't happen");
			}
		}
		y--;
	}
	assert(y == -1);	
}

void read_estimator_output(FILE *f, int ownermap[19 * 19])
{
	char *line, buf[128];
	int n = sizeof(buf);
	while ((line = fgets(buf, n, f))) {
		
		if (*line == '#')  continue;
		if (!strncmp("height: ", line, 8)) {
			boardsize = atoi(line + 8);
			assert(boardsize >= 3);
			assert(boardsize <= 19);
			boardsize2 = boardsize * boardsize;
			continue;
		}
		if (!strncmp("width: ", line, 7))  continue;
		if (!strncmp("player_to_move: ", line, 16))  continue;
		if (boardsize)  break;
	}

	read_board(f, board);
	line = fgets(buf, n, f);
	line = fgets(buf, n, f);
	read_board(f, ownermap);
}

int dead_stones[19 * 19] = { 0, };
int ndead_stones = 0;

void find_dead_stones(int ownermap[19 * 19])
{
	for (int c = 0; c < boardsize2; c++) {
		if (board[c] != S_BLACK &&
		    board[c] != S_WHITE)  continue;
		if (ownermap[c] != other_color(board[c]))  continue;
		dead_stones[ndead_stones++] = c;
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

int
board_score_handicap_compensation()
{
        switch (rules) {             
                /* Usually this makes territory and area scoring the same.
                 * See handicap go section of:
                 * https://senseis.xmp.net/?TerritoryScoringVersusAreaScoring */
		case RULES_JAPANESE:  return (handicap ? handicap - 1 : 0);
		
                /* RULES_CHINESE etc */
		default:              return  handicap;
        }
        
        assert(0);  /* not reached */
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
	int stones[4]    = { 0, };
        int prisoners[4] = { 0, captures[S_BLACK], captures[S_WHITE], 0 };
	const int o[4] = {0, 1, 2, 0};
	for (int c = 0; c < boardsize2; c++) {
		ownermap[c] = o[board[c]];
		stones[board[c]]++;
	}

	/* Process dead stones. */
	for (int i = 0; i < ndead_stones; i++) {
		//foreach_in_group(board, dead_stones[i]) {
		int c = dead_stones[i];
		int color = board[c];
		ownermap[c] = o[other_color(color)];
		stones[color]--; prisoners[other_color(color)]++;
	}

	/* We need to special-case empty board. */
	if (!stones[S_BLACK] && !stones[S_WHITE])
		return komi;

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

        if (rules == RULES_JAPANESE) {  /* XXX Needs to know board captures or this won't work ... */
                int wterritory = scores[S_WHITE] - stones[S_WHITE];
                int bterritory = scores[S_BLACK] - stones[S_BLACK];
                return komi + wterritory + prisoners[S_WHITE] - bterritory - prisoners[S_BLACK];
        }

	int handi_comp = board_score_handicap_compensation(board);
	return komi + handi_comp + scores[S_WHITE] - scores[S_BLACK];
}

bool handicap_set = false;
bool komi_set = false;
bool rules_set = false;
bool cap_black_set = false;
bool cap_white_set = false;

int main(int argc, char **argv)
{
        int opt;
        int option_index;
        /* Leading ':' -> we handle error messages. */
        while ((opt = getopt_long(argc, argv, ":hk:a:r:b:w:", longopts, &option_index)) != -1) {
                switch (opt) {
			case 'a':  handicap_set = true;  handicap = atoi(optarg);       break;
			case 'k':  komi_set = true;      komi = atof(optarg);           break;
			case 'r':  rules_set = true;     rules = str2rules(optarg);     break;
			case 'b':  cap_black_set = true; captures[S_BLACK] = atoi(optarg);  break;
			case 'w':  cap_white_set = true; captures[S_WHITE] = atoi(optarg);  break;
			case 'h':  usage(); exit(0);
			case ':':  die("%s: Missing argument\n"
				       "Try 'score_game --help' for more information.\n", argv[optind-1]);
			default:   die("Invalid argument: %s\n"
				       "Try 'score_game --help' for more information.\n", argv[optind-1]);
		}
	}

	/* Check rules sanity */
	if (rules_set && rules == RULES_CHINESE)
		if (!(komi_set && handicap_set))  die("need to set komi and handicap for chinese rules.");

	if (rules_set && rules == RULES_JAPANESE)
		if (!(komi_set && cap_black_set && cap_white_set))  die("need to set komi and b/w captures for japanese rules.");
	
	if (argc - optind != 0) {  usage(); exit(1);  }

	int ownermap[19 * 19];  /* ownermap from estimator */
	read_estimator_output(stdin, ownermap);
	print_board(board);
	find_dead_stones(ownermap);

	int dame = 0;
	float score = board_official_score_and_dame(&dame);
	printf("Komi: %.1f\n", komi);
	if (handicap_set)   printf("Handicap: %i\n",   handicap);
	if (cap_black_set)  printf("Captures B: %i\n", captures[S_BLACK]);
	if (cap_white_set)  printf("Captures W: %i\n", captures[S_WHITE]);
	printf("Official score: %.1f (%s)\n", -score, rules2str(rules));
	printf("Official dames: %i\n", dame);
	printf("Score: %f\n", -score);
}
