#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#define X_OLD 0
#define Y_OLD 1
#define X_NEW 2
#define Y_NEW 3
#define TOTAL_STEPS_RAN 4
#define NUM_REACH_BALL 5
#define NUM_KICK_BALL 6

#define WIDTH 64
#define LENGTH 128
#define NUM_PLAYER 11
#define MAX_STEP 10
#define NUM_ROUND 900
#define TAG_SEND_BALL_COOR 0
#define TAG_SEND_PLAYER_INFO 1
#define TAG_SEND_WINNER_ID 2
#define SIZE_INFO 7

long long wall_clock_time()
{
#ifdef __linux__
	struct timespec tp;
	clock_gettime(CLOCK_REALTIME, &tp);
	return (long long)(tp.tv_nsec + (long long)tp.tv_sec * 1000000000ll);
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (long long)(tv.tv_usec * 1000 + (long long)tv.tv_sec * 1000000000ll);
#endif
}

int minOf(int x, int y) {
	if (x <= y) return x;
	return y;
}

int isOutOfField(int x, int y) {
	if (x < 0 || y < 0 || x >= LENGTH || y >= WIDTH) return 1;
	return 0;
}

int isFieldProcess(int id) {
	return id == 0;
}

int calDistance(int x1, int y1, int x2, int y2) {
	return abs(x1-x2) + abs(y1-y2);
}

/**
 * get a random number from 0 to n
 */
int randomInt(int n) {
	if (n == 0) return 0;
	return rand() % n;
}

/** 
 * x, y: coordinate of the player
 * xBall, yBall: coordinate of the ball
 * The player will try to reach the ball in 10 steps
 * if he is unable to reach the ball, then he run 10 steps randomly toward the ball
 * The player's new coordinate will be write to xNew and yNew
 * The number of steps player run will be write to steps
 * Return 1 if the player can reach the ball, return 0 otherwise
 */

int move(int x, int y, int xBall, int yBall, int *steps, int *xNew, int *yNew) {
	if (calDistance(x, y, xBall, yBall) <= MAX_STEP) {
		*xNew = xBall;
		*yNew = yBall;
		*steps = calDistance(x, y, xBall, yBall);
		return 1;
	}

	int maxXSteps = minOf(MAX_STEP, abs(x-xBall));
	int minXSteps = MAX_STEP - minOf(MAX_STEP, abs(y-yBall));
	int xSteps = randomInt(maxXSteps - minXSteps);
	xSteps += minXSteps;
	int ySteps = MAX_STEP - xSteps;
	if (xBall > x) {
		*xNew = x + xSteps;
	} else {
		*xNew = x - xSteps;
	}
	if (yBall > y) {
		*yNew = y + ySteps;
	} else {
		*yNew = y - ySteps;
	}
	*steps = MAX_STEP;
	return 0;
}

// Return the id of the ball winner, -1 if noone wins
int getBallWinner(int info[NUM_PLAYER][SIZE_INFO], int xBall, int yBall) {
	int reachedCounter = 0;
	int reachedPlayers[NUM_PLAYER];
	for (int i=0; i<NUM_PLAYER; i++) {
		if (calDistance(info[i][X_NEW], info[i][Y_NEW], xBall, yBall) == 0) {
			reachedPlayers[reachedCounter] = i;
			reachedCounter ++;
		}
	}
	if (reachedCounter == 0) {
		return -1;
	}
	return reachedPlayers[randomInt(reachedCounter)];
}

int main(int argc,char *argv[]) {
	long long startTime = wall_clock_time();
	int numtasks, rank;
	int x[NUM_PLAYER], y[NUM_PLAYER];
	int xBall, yBall, xBallOld, yBallOld, winnerId;
	int id, xOld, yOld, xNew, yNew, stepsRan;
	int totalStepsRan = 0, numReachBall = 0, numKickBall = 0;
	int infoBuffer[SIZE_INFO], playersBuffer[NUM_PLAYER][SIZE_INFO], ballBuffer[2], winnerBuffer[1];
	
	MPI_Request sendReqs[NUM_PLAYER], recvReqs[NUM_PLAYER];
	MPI_Status sendStats[NUM_PLAYER], recvStats[NUM_PLAYER];

	MPI_Init(&argc,&argv);
	MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	srand(rank * time(NULL));
	// Initialize ball and players' coordinate
	if (rank == 0) {
		xBall = randomInt(LENGTH);
		yBall = randomInt(WIDTH);
	} else {
		xOld = randomInt(LENGTH);
		yOld = randomInt(WIDTH);
	}

	for (int i=0; i<NUM_ROUND; i++) {
		// Send and receive ball coordinate
		if (rank == 0) {
			ballBuffer[0] = xBall; ballBuffer[1] = yBall;
			for (int j=0; j<NUM_PLAYER; j++) {
				MPI_Isend(ballBuffer, 2, MPI_INT, j + 1, TAG_SEND_BALL_COOR, MPI_COMM_WORLD, &sendReqs[j]);
			}
			MPI_Waitall(NUM_PLAYER, sendReqs, sendStats);
		}
		if (rank != 0) {
			id = rank - 1;
			MPI_Irecv(ballBuffer, 2, MPI_INT, 0, TAG_SEND_BALL_COOR, MPI_COMM_WORLD, &recvReqs[id]);
			MPI_Waitall(1, &recvReqs[id], recvStats);
		}

		// Player run and send info to field
		if (rank == 0) {
			for (int j=0; j<NUM_PLAYER; j++) {
				MPI_Irecv(playersBuffer[j] , SIZE_INFO, MPI_INT, j + 1, TAG_SEND_PLAYER_INFO, MPI_COMM_WORLD, &recvReqs[j]);
			}
			MPI_Waitall(NUM_PLAYER, recvReqs, recvStats);
		}
		if (rank != 0) {
			xBall = ballBuffer[0]; yBall = ballBuffer[1];
			int reachable = move(xOld, yOld, xBall, yBall, &stepsRan, &xNew, &yNew);
			totalStepsRan += stepsRan;
			numReachBall += reachable;
			infoBuffer[X_OLD] = xOld; infoBuffer[Y_OLD] = yOld; infoBuffer[X_NEW] = xNew; infoBuffer[Y_NEW] = yNew; 
			infoBuffer[TOTAL_STEPS_RAN] = totalStepsRan; infoBuffer[NUM_REACH_BALL] = numReachBall; infoBuffer[NUM_KICK_BALL] = numKickBall;
			MPI_Isend(infoBuffer, SIZE_INFO, MPI_INT, 0, TAG_SEND_PLAYER_INFO, MPI_COMM_WORLD, &sendReqs[id]);
			MPI_Waitall(1, &sendReqs[id], sendStats);
		}

		// // Field decide who get the ball
		if (rank == 0) {
			winnerId = getBallWinner(playersBuffer, xBall, yBall);
			winnerBuffer[0] = winnerId;
			for (int j=0; j<NUM_PLAYER; j++) {
				MPI_Isend(winnerBuffer, 1, MPI_INT, j + 1, TAG_SEND_WINNER_ID, MPI_COMM_WORLD, &sendReqs[j]);
			}
			MPI_Waitall(NUM_PLAYER, sendReqs, sendStats);
		}
		if (rank != 0) {
			MPI_Irecv(winnerBuffer, 1, MPI_INT, 0, TAG_SEND_WINNER_ID, MPI_COMM_WORLD, &recvReqs[id]);
			MPI_Waitall(1, &recvReqs[id], recvStats);
		}

		// Kick the ball
		if (rank == 0) {
			if (winnerId != -1) {
				playersBuffer[winnerId][NUM_KICK_BALL] ++;
				MPI_Irecv(ballBuffer, 2, MPI_INT, winnerId + 1, TAG_SEND_BALL_COOR, MPI_COMM_WORLD, &recvReqs[0]);
				MPI_Waitall(1, &recvReqs[0], recvStats);
			}
		}
		if (rank != 0) {
			if (id == winnerBuffer[0]) {
				numKickBall ++;
				ballBuffer[0] = randomInt(LENGTH); ballBuffer[1] = randomInt(WIDTH);
				MPI_Isend(ballBuffer, 2, MPI_INT, 0, TAG_SEND_BALL_COOR, MPI_COMM_WORLD, &sendReqs[0]);
				MPI_Waitall(1, &sendReqs[0], sendStats);
			}
		}

		// Output
		if (rank == 0) {
			if (winnerId != -1) {
				xBallOld = xBall;
				yBallOld = yBall;
				xBall = ballBuffer[0];
				yBall = ballBuffer[1];
			}
			printf("Round %d\n", i);
			printf("  Ball is at %d %d\n", xBall, yBall);
			for (int j=0; j<NUM_PLAYER; j++) {
				int reached = (calDistance(playersBuffer[j][X_NEW], playersBuffer[j][Y_NEW], xBallOld, yBallOld) == 0) ? 1 : 0;
				int kicked = (j == winnerId) ? 1 : 0;
				printf("    %2d %3d %3d %3d %3d %d %d %4d %3d %3d\n", 
				j, playersBuffer[j][X_OLD], playersBuffer[j][Y_OLD], playersBuffer[j][X_NEW], playersBuffer[j][Y_NEW], 
				reached, kicked, playersBuffer[j][TOTAL_STEPS_RAN], playersBuffer[j][NUM_REACH_BALL], playersBuffer[j][NUM_KICK_BALL]);
			}
		}
		if (rank != 0) {
			xOld = xNew; yOld = yNew;
		}
	}

	MPI_Finalize();
	
	long long endTime = wall_clock_time();
	if (rank == 0) {
		printf("Execution time: %1.2f\n", (endTime - startTime) / 1000000000.0);
	}

	return 0;
}