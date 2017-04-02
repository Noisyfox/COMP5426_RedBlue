#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

#include <mpi.h>

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

#define MASTER_RANK 0


void print_usage(char const* const message, ...)
{
	va_list arg_ptr;
	va_start(arg_ptr, message);

	vfprintf(stderr, message, arg_ptr);

	va_end(arg_ptr);

	// TODO: print usage
}

int parse_argument(int is_master, int argc, char* argv[], int* n, int* t, int* c, int* max_iters)
{
	int i;
	int flags[4] = {0,0,0,0};
	char* arguments[] = {
		"n",
		"t",
		"c",
		"max_iters"
	};

	for (i = 1; i < argc - 1; i += 2)
	{
		if (strcmp(argv[i], "-n") == 0)
		{
			*n = atoi(argv[i + 1]);
			flags[0] = 1;
		}
		else if (strcmp(argv[i], "-t") == 0)
		{
			*t = atoi(argv[i + 1]);
			flags[1] = 1;
		}
		else if (strcmp(argv[i], "-c") == 0)
		{
			*c = atoi(argv[i + 1]);
			flags[2] = 1;
		}
		else if (strcmp(argv[i], "-max_iters") == 0)
		{
			*max_iters = atoi(argv[i + 1]);
			flags[3] = 1;
		}
		else
		{
			if (is_master)
			{
				print_usage("Unknown argument \"%s\"", argv[i]);
			}
			return 0;
		}
	}

	// Make sure all arguments are set
	for (i = 0; i < 4; i++)
	{
		if (!flags[i])
		{
			if (is_master)
			{
				print_usage("Argument \"%s\" not set!", arguments[i]);
			}
			return 0;
		}
	}

	return 1;
}


int** board_init(int n)
{
	int count = n * n;
	int* board_flat = malloc(sizeof(int) * count);
	int** board = malloc(sizeof(int*) * n);
	int i;

	// build 2d-array
	board[0] = board_flat;
	for (i = 1; i < n; i++)
	{
		board[i] = &board_flat[i * n];
	}

	// init board
	for(i = 0; i < count; i++)
	{
		// after that the board will be filled as 0 1 2 0 1 2 0 1 2 ...
		// so is roughly 1/3 cells in read, 1/3 in white and 1/3 in blue
		board_flat[i] = i % 3;
	}

	// then we shuffle the board to get a random order
	srand(time(NULL));
	if (count > 1)
	{
		for (i = 0; i < count - 1; i++)
		{
			int j = i + rand() / (RAND_MAX / (count - i) + 1);
			int t = board_flat[j];
			board_flat[j] = board_flat[i];
			board_flat[i] = t;
		}
	}

	return board;
}


/*
 * Create sub chessboard with two extra rows
 * with following structure:
 * 
 * -----------------------------------------
 * |          | 0 | 1 | 2 | ... | cols - 1 |
 * |----------+---+---+---+-----+----------|
 * |-1        | last row from prev process |
 * |----------+----------------------------|
 * | 0        |                            |
 * |----------+                            |
 * | 1        |                            |
 * |----------+                            |
 * | ...      |  rows for current process  |
 * |----------+                            |
 * | rows - 2 |                            |
 * |----------+                            |
 * | rows - 1 |                            |
 * |----------+----------------------------|
 * | rows     |first row from prev process |
 * -----------------------------------------
 */
int** create_sub_board(int rows, int cols)
{
	int count = (rows + 2) * cols;
	int* board_flat = malloc(sizeof(int) * count);
	int** board = malloc(sizeof(int*) * (rows + 2));
	int i;

	memset(board_flat, 0, sizeof(int) * count);

	// build 2d-array
	board[0] = board_flat;
	for (i = 1; i < rows + 2; i++)
	{
		board[i] = &board_flat[i * cols];
	}

	return &board[1];
}


void print_board(int** board, int row_count, int col_count, int include_overflow)
{
	int i, j;
	
	if(include_overflow)
	{
		for (j = 0; j < col_count; j++)
		{
			printf(" %d ", board[-1][j]);
		}
		printf("\n");

		for (j = 0; j < col_count; j++)
		{
			printf("---");
		}
		printf("\n");
	}

	for(i = 0; i < row_count; i++)
	{
		for(j = 0; j < col_count; j++)
		{
			printf(" %d ", board[i][j]);
		}
		printf("\n");
	}

	if (include_overflow)
	{
		for (j = 0; j < col_count; j++)
		{
			printf("---");
		}
		printf("\n");

		for (j = 0; j < col_count; j++)
		{
			printf(" %d ", board[row_count][j]);
		}
		printf("\n");
	}
	printf("\n");
}


/*
 * Calculate how may rows should process X takes
 */
int row_count_for_process(int process_count, int row_count, int tile_row, int current_id)
{
	int tile_count = row_count / tile_row;
	int remain = tile_count % process_count;
	int base = tile_count / process_count;

	int row = base * tile_row;

	if(current_id < remain)
	{
		row += tile_count;
	}

	return row;
}


void do_red(int** sub_board, int row_count, int col_count)
{
	int i, j;

	for (i = 0; i < row_count; i++)
	{
		if (sub_board[i][0] == RED && sub_board[i][1] == WHITE)
		{
			sub_board[i][0] = OUT;
			sub_board[i][1] = IN;
		}

		for (j = 1; j < col_count; j++)
		{
			int right = (j + 1) % col_count;

			if (sub_board[i][j] == RED && sub_board[i][right] == WHITE)
			{
				sub_board[i][j] = WHITE;
				sub_board[i][right] = IN;
			}
			else if (sub_board[i][j] == IN)
			{
				sub_board[i][j] = RED;
			}
		}
		if (sub_board[i][0] == IN)
		{
			sub_board[i][0] = RED;
		}
		else if (sub_board[i][0] == OUT)
		{
			sub_board[i][0] = WHITE;
		}
	}
}


int main(int argc, char* argv[])
{
	int my_id, num_procs, prev_id, next_id;
	int is_master;

	// User defined parameters
	int n, t, c, max_iters;

	int** full_chessboard; // for master process only, full_chessboard[row][col]
	int** my_rows; // the rows for current process, my_rows[row][col]
	int row_count; // row count for current process

	int i, j;
	MPI_Status status;

	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
	MPI_Comm_rank(MPI_COMM_WORLD, &my_id);

	is_master = my_id == MASTER_RANK;
	prev_id = is_master ? num_procs - 1 : my_id - 1;
	next_id = (my_id + 1) % num_procs;

	// First parse arguments
	if (!parse_argument(is_master, argc, argv, &n, &t, &c, &max_iters))
	{
		goto _exit;
	}

	// Init some process-related vars
	row_count = row_count_for_process(num_procs, n, t, my_id);
	my_rows = create_sub_board(row_count, n);

	// Initialize the chessboard
	if (is_master)
	{
		// The master process take charge of generating chessboard
		// and send them to other process(es).

		full_chessboard = board_init(n);

		printf("Original chessboard:\n");
		print_board(full_chessboard, n, n, 0);

		// First allocate rows for itself
		for (i = 0; i < row_count; i++)
		{
			memcpy(my_rows[i], full_chessboard[i], sizeof(int) * n);
		}

		// Next, send rows to slave processes
		for (j = 1; j < num_procs; j++)
		{
			int k = row_count_for_process(num_procs, n, t, j);

			MPI_Send(full_chessboard[i], k * n, MPI_INT, j, 0, MPI_COMM_WORLD);
		}
	}
	else
	{
		// Slave processes receive rows from master process.
		MPI_Recv(my_rows[0], row_count * n, MPI_INT, MASTER_RANK, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
	}

	printf("Chessboard at process %d:\n", my_id);
	print_board(my_rows, row_count, n, 1);

	// Now the step starts:

	// First, move red blocks
	do_red(my_rows, row_count, n);

	// Then we need to sync with all processes
	// Thanks for the genious MPI_Sendrecv() so we won't get a dead lock!
	MPI_Sendrecv(my_rows[0], n, MPI_INT, prev_id, 0, my_rows[row_count], n, MPI_INT, next_id, 0, MPI_COMM_WORLD, &status);
	MPI_Sendrecv(my_rows[row_count - 1], n, MPI_INT, next_id, 0, my_rows[-1], n, MPI_INT, prev_id, 0, MPI_COMM_WORLD, &status);

	print_board(my_rows, row_count, n, 1);

_exit:
	MPI_Finalize();
	return 0;
}
