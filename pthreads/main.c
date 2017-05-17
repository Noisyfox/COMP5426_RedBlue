#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
#include <pthread.h>

/*
white = 0, red = 1, blue = 2,
red or blue just moved in = 3 and
red or blue (in the first row or column) just moved out = 4
*/

#define WHITE 0
#define RED 1
#define BLUE 2
#define IN 3
#define OUT 4


void usage(char const *const message, ...) {
    va_list arg_ptr;
    va_start(arg_ptr, message);

    vfprintf(stderr, message, arg_ptr);

    va_end(arg_ptr);

    fprintf(stderr, "\n");

    fprintf(stderr, "Please run the application in the following format:\n");
    fprintf(stderr, "red_blue [thread num] [grid size] [tile size] [threshold (float)] [maximum iterations]\n");
    fprintf(stderr, "Required: [grid size] %% [tile size] = 0; 1 <= [thread num] <= [grid size] / [tile size]\n");
    fprintf(stderr, "\n");
}

int **create_board(int n) {
    int count = n * n;
    int *board_flat = malloc(sizeof(int) * count);
    int **board = malloc(sizeof(int *) * n);
    int i;

    // build 2d-array
    board[0] = board_flat;
    for (i = 1; i < n; i++) {
        board[i] = &board_flat[i * n];
    }

    return board;
}

int **board_init(int n) {
    int count = n * n;
    int i;
    int **board = create_board(n);

    // init board
    for (i = 0; i < count; i++) {
        // after that the board will be filled as 0 1 2 0 1 2 0 1 2 ...
        // so is roughly 1/3 cells in read, 1/3 in white and 1/3 in blue
        board[0][i] = i % 3;
    }

    // then we shuffle the board to get a random order
    srand(time(NULL));
    if (count > 1) {
        for (i = 0; i < count - 1; i++) {
            int j = i + rand() / (RAND_MAX / (count - i) + 1);
            int t = board[0][j];
            board[0][j] = board[0][i];
            board[0][i] = t;
        }
    }

    return board;
}


/*
 * Calculate how may rows should process X takes
 */
int row_count_for_process(int process_count, int row_count, int tile_row, int current_id) {
    int tile_count = row_count / tile_row;
    int remain = tile_count % process_count;
    int base = tile_count / process_count;

    int row = base * tile_row;

    if (current_id < remain) {
        row += tile_count;
    }

    return row;
}

void do_red(int **board, int row_start, int row_end, int n) {
    for (int i = row_start; i <= row_end; i++) {
        if (board[i][0] == RED && board[i][1] == WHITE) {
            board[i][0] = OUT;
            board[i][1] = IN;
        }

        for (int j = 1; j < n; j++) {
            int right = (j + 1) % n;

            if (board[i][j] == RED && board[i][right] == WHITE) {
                board[i][j] = WHITE;
                board[i][right] = IN;
            } else if (board[i][j] == IN) {
                board[i][j] = RED;
            }
        }
        if (board[i][0] == IN) {
            board[i][0] = RED;
        } else if (board[i][0] == OUT) {
            board[i][0] = WHITE;
        }
    }
}

void do_blue(int **board, int col_start, int col_end, int n) {
    for (int j = col_start; j <= col_end; j++) {
        if (board[0][j] == BLUE && board[1][j] == WHITE) {
            board[0][j] = OUT;
            board[1][j] = IN;
        }

        for (int i = 1; i < n; i++) {
            int below = (i + 1) % n;

            if (board[i][j] == BLUE && board[below][j] == WHITE) {
                board[i][j] = WHITE;
                board[below][j] = IN;
            } else if (board[i][j] == IN) {
                board[i][j] = BLUE;
            }
        }
        if (board[0][j] == IN) {
            board[0][j] = BLUE;
        } else if (board[0][j] == OUT) {
            board[0][j] = WHITE;
        }
    }
}


// Simple Barrier implementation
typedef struct {
    int count;
    int current;
    unsigned int phase;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} barrier_t;

void barrier_init(barrier_t *barrier, int count) {
    barrier->count = count;
    barrier->current = 0;
    barrier->phase = 0;

    pthread_mutex_init(&barrier->mutex, NULL);
    pthread_cond_init(&barrier->cond, NULL);
}

void barrier_destroy(barrier_t *barrier) {
    pthread_mutex_destroy(&barrier->mutex);
    pthread_cond_destroy(&barrier->cond);
}

void barrier_wait(barrier_t *barrier) {
    pthread_mutex_lock(&barrier->mutex);

    barrier->current++;
    if (barrier->current >= barrier->count) {
        barrier->phase++;
        barrier->current = 0;
        pthread_cond_broadcast(&barrier->cond);
    } else {
        unsigned int phase = barrier->phase;
        do {
            pthread_cond_wait(&barrier->cond, &barrier->mutex);
        } while (phase != barrier->phase);
    }

    pthread_mutex_unlock(&barrier->mutex);
}

// User defined parameters
typedef struct {
    int n, t, max_iters;
    int p;
    double c;
} parameters;

typedef struct {
    barrier_t barrier;
} thread_sync;

typedef struct {
    thread_sync *sync;
    parameters *params;

    int id;
    int **board;
    int start;
    int end;
} thread_context;


void *process_thread(void *arg) {
    thread_context *ctx = (thread_context *) arg;
    parameters *params = ctx->params;

    int n = params->n;
    int t = params->t;
    int max_iters = params->max_iters;
    int p = params->p;
    double c = params->c;

    int **board = ctx->board;
    int start = ctx->start;
    int end = ctx->end;

    int iter_count = 0;
    while (iter_count++ <= max_iters) {
        do_red(board, start, end, n);
        barrier_wait(&ctx->sync->barrier);
        do_blue(board, start, end, n);

        // TODO: Check result
    }

    return NULL;
}


int main(int argc, char *argv[]) {
    parameters params;

    // Read user input
    if (argc < 6) {
        usage("Too few arguments (5 required, %d provided).", argc - 1);
        goto _exit;
    }

    params.p = atoi(argv[1]);
    params.n = atoi(argv[2]);
    params.t = atoi(argv[3]);
    params.c = atof(argv[4]);
    params.max_iters = atoi(argv[5]);

    if (params.n % params.t != 0) {
        usage("Illegal arguments: [grid size] %% [tile size] != 0");
        goto _exit;
    }

    if (params.p < 1) {
        usage("Illegal arguments: [thread num] < 1");
        goto _exit;
    }

    if (params.p > params.n / params.t) {
        usage("Illegal arguments: [thread num] > [grid size] / [tile size]");
        goto _exit;
    }

    int **board = board_init(params.n); // Create a random board

    if (params.p > 1) {
        // Multi-threading computation
        int **board_copy = create_board(params.n);
        memcpy(board_copy[0], board[0], params.n * params.n * sizeof(int));

        // Allocate async context
        thread_sync sync;
        thread_context *context = malloc(params.p * sizeof(thread_context));
        pthread_t *threads = malloc((params.p - 1) * sizeof(pthread_t));

        // Init thread contexts
        barrier_init(&sync.barrier, params.p);
        int rp = 0;
        for (int i = 0; i < params.p; i++) {
            thread_context *ctx = &context[i];
            ctx->sync = &sync;
            ctx->params = &params;
            ctx->board = board_copy;
            ctx->id = i;
            ctx->start = rp;
            rp += row_count_for_process(params.p, params.n, params.t, i);
            ctx->end = rp - 1;
        }

        // Start threads
        for (int i = 1; i < params.p; i++) {
            pthread_create(&threads[i - 1], NULL, process_thread, &context[i]);
        }
        process_thread(&context[0]); // This thread runs as t0

        // Wait for all thread exit
        for (int i = 0; i < params.p - 1; i++) {
            pthread_join(threads[i], NULL);
        }

        // TODO: Read result from thread_sync

        // Release data
        free(context);
        free(threads);
        free(board_copy);
        barrier_destroy(&sync.barrier);
    }

    // Serial computation
    int iter_count = 0;
    while (iter_count++ <= params.max_iters) {
        do_red(board, 0, params.n - 1, params.n);
        do_blue(board, 0, params.n - 1, params.n);

        // TODO: Check result
    }

    free(board);

    _exit:
    return 0;
}
