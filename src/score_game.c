/* score_game:
 * Reads ogs score estimator output and scores game.
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
#include <getopt.h>
#include <math.h>

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
        RULES_AGA,
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

#define MIN(a, b) ((a) < (b) ? (a) : (b));

#define board_large()  (boardsize >= 15)
#define board_small()  (boardsize <= 9)

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
#define coord_x(c)      ((c) % boardsize)
#define coord_y(c)      ((c) / boardsize)

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

int
group_liberties_(int coord, int color, int *map, int n, int max_libs)
{
	foreach_neighbor(coord, {
		if (!board[c] && !map[c]) {  /* new lib */
			map[c] = 1;
			if (++n >= max_libs)  return n;
		}
	});

	foreach_neighbor(coord, {
		if (board[c] == color && !map[c]) {
			map[c] = -1;
			n = group_liberties_(c, color, map, n, max_libs);
			if (n >= max_libs)  return n;
		}
	});
	return n;
}

int
group_liberties(int coord, int *libs, int max_libs)
{
	int map[19*19] = { 0, };
	int color = board[coord];
	assert(color);
	int n = group_liberties_(coord, color, map, 0, max_libs);

	int i = 0;
	for (int c = 0; c < boardsize2; c++) {
		if (map[c] == 1)
			libs[i++] = c;
	}
	return n;
}

void read_board(FILE *f, int *board)
{
	assert(boardsize);
	char *line, buf[128];
	int n = sizeof(buf);
	int y = boardsize - 1;
	while (y >= 0) {
		line = fgets(buf, n, f);
		if (strlen(line) != boardsize * 2 &&
		    strlen(line) != boardsize * 2 + 1)
			die("board line %i is not %i char long:\n%s\n", boardsize - y, boardsize * 2, line);

		assert(y >= 0);
		for (int x = 0; x < boardsize; x++) {
			int c = y * boardsize + x;
			switch (line[x * 2]) {
				case '.':  board[c] = S_NONE;  break;
				case 'X':  board[c] = S_BLACK; break;
				case 'O':  board[c] = S_WHITE; break;
				default:  die("bad stone: '%c'", line[x * 2]);
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
                if (!strncmp("boardsize ", line, 10)) {
                        boardsize = atoi(line + 10);
                        assert(boardsize >= 0);
                        assert(boardsize <= 19);
                        boardsize2 = boardsize * boardsize;
			
                        read_board(f, board);
			
			line = fgets(buf, n, f);
			line = fgets(buf, n, f);
			read_board(f, ownermap);			
			return;
                }
        }
}

/* Find dead and unclear stones. */
void find_dead_unclear_stones(int ownermap[19 * 19],
			      int dead[], int *ndead,
			      int unclear[], int *nunclear)
{
	for (int c = 0; c < boardsize2; c++) {
		if (board[c] != S_BLACK &&
		    board[c] != S_WHITE)  continue;
		if (ownermap[c] == S_NONE)                  unclear[(*nunclear)++] = c;
		if (ownermap[c] == other_color(board[c]))   dead[(*ndead)++] = c;
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

int board_score_handicap_compensation()
{
        switch (rules) {             
                /* Usually this makes territory and area scoring the same.
                 * See handicap go section of:
                 * https://senseis.xmp.net/?TerritoryScoringVersusAreaScoring */
		case RULES_JAPANESE:
		case RULES_AGA:       return (handicap ? handicap - 1 : 0);
		
                /* RULES_CHINESE etc */
		default:              return  handicap;
        }
        
        assert(0);  /* not reached */
}

/* Compute score estimate from mc ownermap */
float board_ownermap_score_est(int *ownermap, int *unclear_stones, int *unclear_empty)
{
	float scores[4] = {0.0, };  /* Number of points owned by each color */
        if (unclear_stones)  *unclear_stones = 0;

	for (int c = 0; c < boardsize2; c++) {
		int color = ownermap[c];
		
		/* If status is unclear and there's a stone there assume it's alive. */
		if (color == S_NONE && (board[c] == S_BLACK || board[c] == S_WHITE)) {
			color = board[c];  (*unclear_stones)++;
		}
		
                scores[color]++;
        }

	*unclear_empty = scores[S_NONE];

        int handi_comp = board_score_handicap_compensation();
        return ((scores[S_WHITE] + komi + handi_comp) - scores[S_BLACK]);
}

/* Official score after removing dead groups and Tromp-Taylor counting.
 * Number of dames is saved in @dame, final ownermap in @ownermap. */
float board_official_score_details(int *dead, int ndead,
				   int *dame, int *ownermap)
{
	/* A point P, not colored C, is said to reach C, if there is a path of
	 * (vertically or horizontally) adjacent points of P's color from P to
	 * a point of color C.
	 *
	 * A player's score is the number of points of her color, plus the
	 * number of empty points that reach only her color. */
	
	int stones[4]    = { 0, };
        int prisoners[4] = { 0, captures[S_BLACK], captures[S_WHITE], 0 };
	const int o[4] = {0, 1, 2, 0};
	for (int c = 0; c < boardsize2; c++) {
		ownermap[c] = o[board[c]];
		stones[board[c]]++;
	}

	/* Process dead stones. */
	for (int i = 0; i < ndead; i++) {
		int c = dead[i];
		int color = board[c];
		ownermap[c] = o[other_color(color)];
		stones[color]--; prisoners[other_color(color)]++;
	}

	/* We need to special-case empty board. */
	if (!stones[S_BLACK] && !stones[S_WHITE])
		return komi;

	while (board_tromp_taylor_iter(ownermap))
		/* Flood-fill... */;

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

bool
border_stone(int coord, int *final_ownermap)
{
	int color = board[coord];
	foreach_neighbor(coord, {
		if (board[c] == other_color(color) &&
		    final_ownermap[c] == other_color(color))
			return true;
	});
	return false;
}

bool
board_position_final(float official_score, int *final_ownermap, int dames,
		     float score_est, int nunclear,
		     char **msg)
{
	//*msg = "too early to pass";
	//if (b->moves < board_earliest_pass(b))
	//	return false;
	
	/* Unclear groups ? */
	*msg = "unclear groups";
	if (nunclear)  return false;

	/* Border stones in atari ? */
	for (int c = 0; c < boardsize2; c++) { // foreach_point()
		if (!board[c])  continue;
		int lib[10];
		if (group_liberties(c, lib, 2) > 1)  continue;
		if (!border_stone(c, final_ownermap))  continue;
		int g = c;

		int color = board[c];
		foreach_neighbor(lib[0], {
			if (final_ownermap[c] != color) continue;
			static char buf[100];
			sprintf(buf, "border stones in atari at %i-%i", coord_x(g), coord_y(g));
			*msg = buf;
			return false;
		});
	}

	/* Non-seki dames surrounded by only dames / border / one color are no dame to me,
	 * most likely some territories are still open ... */
	for (int c = 0; c < boardsize2; c++) {
		if (final_ownermap[c] != 3)  continue;

		int dame = c;
		int around[4] = { 0, };
		foreach_neighbor(dame, {
			around[final_ownermap[c]]++;
		});
		if (around[S_BLACK] + around[3] == 4 ||
		    around[S_WHITE] + around[3] == 4) {
			static char buf[100];
			sprintf(buf, "non-final position at %i-%i", coord_x(dame), coord_y(dame));
			*msg = buf;
			return false;
		}
	}

	/* If ownermap and official score disagree position is likely not final.
	 * If too many dames also. */
	int max_dames = (board_large() ? 15 : 7);
	*msg = "non-final position: too many dames";
	if (dames > max_dames)    return false;
	
	/* Can disagree up to dame points, as long as there are not too many.
	 * For example a 1 point difference with 1 dame is quite usual... */
	int max_diff = MIN(dames, 4);
	*msg = "non-final position: score est and official score don't agree";
	if (fabs(official_score - score_est) > max_diff)  return false;
	
	return true;
}


/* Check position is final and safe to score:
 * No unclear groups and official chinese score and score est agree. */
static bool
game_safe_to_score(float score_est, 
		   int dead[], int ndead, int unclear[], int nunclear,
		   char **msg)
{
	/* Get chinese-rules official score. */
	int dames;
	int final_ownermap[19 * 19];
	enum go_ruleset saved_rules = rules;
	if (rules == RULES_JAPANESE)  rules = RULES_AGA;  /* Can't compare japanese and chinese scores */
	float  official_score = board_official_score_details(dead, ndead, &dames, final_ownermap);
	rules = saved_rules;
	
	return board_position_final(official_score, final_ownermap, dames,
				    score_est, nunclear, msg);
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

	int mc_ownermap[19 * 19];       /* ownermap from estimator */
	int dead[19 * 19] = { 0, };     /* dead stones list */
	int ndead = 0;
	int unclear[19 * 19] = { 0, };  /* unclear stones list */
	int nunclear = 0;
	read_estimator_output(stdin, mc_ownermap);
	print_board(board);
	find_dead_unclear_stones(mc_ownermap, dead, &ndead, unclear, &nunclear);

	int dame = 0;
	int final_ownermap[19 * 19];
	float score = board_official_score_details(dead, ndead, &dame, final_ownermap);
	print_board(final_ownermap);

	int mc_unclear_stones, mc_unclear_empty;
	float mc_score = board_ownermap_score_est(mc_ownermap, &mc_unclear_stones, &mc_unclear_empty);

        printf("MC score: %.1f\n", -mc_score);
        printf("MC unclear stone: %i\n", mc_unclear_stones);
        printf("MC unclear empty: %i\n", mc_unclear_empty);

	printf("Komi: %.1f\n", komi);
	if (handicap_set)   printf("Handicap: %i\n",   handicap);
	if (cap_black_set)  printf("Captures B: %i\n", captures[S_BLACK]);
	if (cap_white_set)  printf("Captures W: %i\n", captures[S_WHITE]);
	printf("Official score: %.1f (%s)\n", -score, rules2str(rules));
	printf("Official dames: %i\n", dame);

	char *msg;
	bool safe_to_score = game_safe_to_score(mc_score, dead, ndead, unclear, nunclear, &msg);
	if (safe_to_score)  printf("Safe to score: %i\n", safe_to_score);
	else                printf("Safe to score: %i (%s)\n", safe_to_score, msg);

	printf("Score: %f\n", -score);
}
