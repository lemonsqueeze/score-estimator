#define DEBUG 1
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctime>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "estimator.h"

using namespace score_estimator;

void die(const char *str, ...) {
	va_list ap;
	va_start(ap, str);
	vfprintf(stderr, str, ap);
	va_end(ap);
	exit(1);
}

void scan1(FILE *fp, const char *fmt, void *param) {
    if (fscanf(fp, fmt, param) != 1) {
        fprintf(stderr, "Failed to parse game file [%s]\n", fmt);
        exit(1);
    }
}

int main(int argn, const char *args[]) {
    srand(time(NULL));
    clock_t start, stop;

    if (argn < 2) {
        fprintf(stderr, "Usage: estimator <file.game> ...\n");
        return -1;
    }

    for (int arg=1; arg < argn; ++arg) {

        FILE *fp = fopen(args[arg], "r");
        if (!fp) {
            fprintf(stderr, "Failed to open file %s\n", args[arg]);
            return -1;
        }

        int player_to_move;
        Goban goban;
	scan1(fp, "boardsize %d\n", &goban.height);
	goban.width = goban.height;

	int stone2int[256];
	for (int i = 0; i < 256; i++)  stone2int[i] = -2;
	stone2int['X'] = 1;
	stone2int['O'] = -1;
	stone2int['.'] = 0;
        for (int y=0; y < goban.height; ++y) {
	    char buf[256];
	    char *line = fgets(buf, sizeof(buf), fp);
	    if (!line)  die("Invalid board\n");
	    if ((int)strlen(line) != goban.width * 2)
		    die("Board line %i not %i characters long\n", y+1, goban.width * 2);
            for (int x=0; x < goban.width; ++x) {
		    unsigned char c = line[x * 2];
		    if (stone2int[c] == -2)  die("Line %i: invalid stone '%c'\n", y+1, c);
		    goban.board[y][x] = stone2int[c];
            }
        }

	printf("boardsize %i\n", goban.width);
        goban.print();
        printf("\n\n");
        start = clock();

        Goban est = goban.estimate(WHITE, 10000, 0.35);
        stop = clock();

        est.print();
        printf("\n\n");
        printf("Score: %f\n", est.score() - 0.5);
        double elapsed_secs = double(stop - start) / CLOCKS_PER_SEC;
        printf("Time elapsed: %f ms\n", elapsed_secs * 750.0);
        if (elapsed_secs > 1.0) {
            printf(">>> WARNING: Estimator took too long to produce a result <<<\n");
        }
    }
}
