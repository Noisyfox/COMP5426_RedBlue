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


void print_board(int** board, int row_count, int col_count)
{
	int i, j;

	for(i = 0; i < row_count; i++)
	{
		for(j = 0; j < col_count; j++)
		{
			printf("%d ", board[i][j]);
		}
		printf("\n");
	}
}


int main(int argc, char* argv[])
{
	int myid, numProcs;
	int is_master;

	// User defined parameters
	int n, t, c, max_iters;

	int** full_chessboard; // for master process only, full_chessboard[row][col]
	int** my_rows; // the rows for current process, my_rows[row][col]

	int i;

	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &numProcs);
	MPI_Comm_rank(MPI_COMM_WORLD, &myid);

	is_master = myid == MASTER_RANK;

	// First parse arguments
	if(!parse_argument(is_master, argc, argv, &n, &t, &c, &max_iters))
	{
		goto _exit;
	}

	// Initialize the chessboard
	if (is_master)
	{
		// The master process take charge of generating chessboard
		// and send them to other process(es).

		full_chessboard = board_init(n);

		print_board(full_chessboard, n, n);
	}
	else
	{
		// Slave processes receive rows from master process.
	}

_exit:
	MPI_Finalize();
	return 0;
}
