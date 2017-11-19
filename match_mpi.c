#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define WIDTH 96
#define LENGTH 128
#define PATCH_SIZE 32
#define GRID_WIDTH 3
#define GRID_LENGTH 4
#define NUM_PLAYER_PER_TEAM 11
#define NUM_TEAM 2
#define TOTAL_ATTRIBUTE 15
#define MIN_ATTRIBUTE 1
#define MAX_ATTRIBUTE 10
#define NUM_ATTRIBUTE 3
#define MAX_STEP 10
#define NUM_ROUND_PER_HALF 1
#define X 0
#define Y 1
#define SPEED 0
#define DRIBBING 1
#define KICK 2
#define INF 1000000

int minOf(int x, int y) {
	if (x <= y) return x;
	return y;
}

int isOutOfField(int x, int y) {
	if (x < 0 || y < 0 || x >= LENGTH || y >= WIDTH) return 1;
	return 0;
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

void initiateAttribute(int attribute[NUM_ATTRIBUTE]) {
	attribute[SPEED] = MIN_ATTRIBUTE + randomInt(MAX_ATTRIBUTE - MIN_ATTRIBUTE);
	int maxDribbing = minOf(MAX_ATTRIBUTE, TOTAL_ATTRIBUTE - attribute[SPEED] - MIN_ATTRIBUTE);
	attribute[DRIBBING] = MIN_ATTRIBUTE + randomInt(maxDribbing - MIN_ATTRIBUTE);
	attribute[KICK] = TOTAL_ATTRIBUTE - attribute[SPEED] - attribute[DRIBBING];
}

int isInsidePatch(int row, int col, int ball[2]) {
	int minX = col * PATCH_SIZE, minY = row * PATCH_SIZE;
	int maxX = minX + PATCH_SIZE - 1, maxY = minY + PATCH_SIZE - 1;
	return (ball[X] >= minX) && (ball[X] <= maxX) && (ball[Y] >= minY) && (ball[Y] <= maxY);
}

int getPatch(int coor[2]) {
	int x = coor[X], y = coor[Y];
	int r = y / PATCH_SIZE, c = x / PATCH_SIZE;
	return r * GRID_LENGTH + c;
}

int maxChasableDistance(int speed) {
	if (2 * speed < MAX_STEP) return 2 * speed;
	return MAX_STEP;
}

int getPlayerProcessId(int teamId, int rankInTeam) {
	return GRID_LENGTH * GRID_WIDTH + teamId * NUM_PLAYER_PER_TEAM + rankInTeam;
}

int getBallChaserIdInTeam(int expectedRoundToCatch[NUM_PLAYER_PER_TEAM]) {
	int mini = INF, res = -1;
	for (int i=0; i<NUM_PLAYER_PER_TEAM; i++) {
		if (expectedRoundToCatch[i] < mini) {
			mini = expectedRoundToCatch[i];
			res = i;
		}
	}
	return res;
}

int moveToBall(int coor[2], int ball[2],int maxChasableDistance, int *xNew, int *yNew) {
	int x = coor[X], y = coor[Y], xBall = ball[X], yBall = ball[Y];
	if (calDistance(x, y, xBall, yBall) <= maxChasableDistance) {
		*xNew = xBall;
		*yNew = yBall;
		return 1;
	}

	int maxXSteps = minOf(maxChasableDistance, abs(x-xBall));
	int minXSteps = maxChasableDistance - minOf(maxChasableDistance, abs(y-yBall));
	int xSteps = randomInt(maxXSteps - minXSteps);
	xSteps += minXSteps;
	int ySteps = maxChasableDistance - xSteps;
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
	return 0;
}

int main(int argc,char *argv[]) {
	int numtasks, rank;
	int ball[2], players[NUM_TEAM][NUM_PLAYER_PER_TEAM][2], expectedRoundToCatch[NUM_PLAYER_PER_TEAM], ballChallenge[NUM_TEAM][NUM_PLAYER_PER_TEAM];
	int isFieldProcess = -1, teamId = -1, rankInTeam = -1, row = -1, col = -1;
	int attribute[NUM_ATTRIBUTE], distToBall, maxChasableSteps, ballChaserId;

	MPI_Init(&argc,&argv);
	MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	srand(rank * time(NULL));
	isFieldProcess = rank < GRID_WIDTH * GRID_LENGTH;
	if (isFieldProcess) {
		row = rank / GRID_LENGTH;
		col = rank % GRID_LENGTH;
	}
	if (!isFieldProcess) {
		teamId = (rank - GRID_WIDTH * GRID_LENGTH ) / NUM_PLAYER_PER_TEAM;
		rankInTeam = (rank - GRID_WIDTH * GRID_LENGTH) % NUM_PLAYER_PER_TEAM;
	}

	int fieldRanks[GRID_WIDTH * GRID_LENGTH], teamRanks[NUM_TEAM][NUM_PLAYER_PER_TEAM];
	for (int i=0; i<GRID_WIDTH; i++) {
		for (int j=0; j<GRID_LENGTH; j++) {
			fieldRanks[i * GRID_LENGTH + j] = i * GRID_LENGTH + j;
		}
	}
	for (int i=0; i<NUM_TEAM; i++) {
		for (int j=0; j<NUM_PLAYER_PER_TEAM; j++) {
			teamRanks[i][j] = GRID_WIDTH * GRID_LENGTH + i * NUM_PLAYER_PER_TEAM + j;
		}
	}

	MPI_Group worldGroup, fieldGroup , teamGroup[NUM_TEAM], thisProcessGroup;
	MPI_Comm fieldComm, teamComm[NUM_TEAM];
	MPI_Comm_group(MPI_COMM_WORLD, &worldGroup);
	MPI_Group_incl(worldGroup, GRID_WIDTH * GRID_LENGTH, fieldRanks, &fieldGroup);
	MPI_Comm_create(MPI_COMM_WORLD, fieldGroup, &fieldComm);
	for (int i=0; i<NUM_TEAM; i++) {
		MPI_Group_incl(worldGroup, NUM_PLAYER_PER_TEAM, teamRanks[i], &teamGroup[i]);
		MPI_Comm_create(MPI_COMM_WORLD, teamGroup[i], &teamComm[i]);
	}
	// MPI_Group_incl(world_group, 1, &rank, &thisProcessGroup);

	// int tmp1=-1, tmp2=-1;
	// if (teamComm[0] != MPI_COMM_NULL) {
	// 	MPI_Comm_size(teamComm[0], &tmp1);
	// 	MPI_Comm_rank(teamComm[0], &tmp2);
	// }
	// printf("%d %d %d %d %d \n", rank, teamId, rankInTeam, tmp1, tmp2);
	
	// Field process 0 initiate ball position 
	if (isFieldProcess) {
		ball[X] = LENGTH / 2; ball[Y] = WIDTH / 2;
	} else {
		players[teamId][rankInTeam][X] = randomInt(LENGTH);
		players[teamId][rankInTeam][Y] = randomInt(WIDTH);
		maxChasableSteps = maxChasableDistance(attribute[SPEED]);
	}

	if (!isFieldProcess) {
		initiateAttribute(attribute);
	}
	
	// if (MPI_COMM_NULL == teamComm[1]) printf("%d fuck\n", rank);
	
	// for (int i=0; i<GRID_WIDTH; i++) {
	// 	for (int j=0; j<GRID_LENGTH; j++) {
	// 		if (rank == 0) printf("%d %d, i %d, j %d : %d\n", ball[0], ball[1], i, j, isInsidePatch(i, j, ball));
	// 	}
	// }
	
	for (int i=0; i<NUM_ROUND_PER_HALF; i++) {
		MPI_Bcast(&ball, 2, MPI_INT, 0, MPI_COMM_WORLD);
		MPI_Barrier(MPI_COMM_WORLD);
				
		if (!isFieldProcess) {
			distToBall = calDistance(ball[X], ball[Y], players[teamId][rankInTeam][X], players[teamId][rankInTeam][Y]);
			expectedRoundToCatch[rankInTeam] = distToBall / maxChasableSteps;
			if (distToBall % maxChasableSteps != 0) expectedRoundToCatch[rankInTeam] ++;
			for (int j=0; j<NUM_PLAYER_PER_TEAM; j++) {
				MPI_Bcast(&expectedRoundToCatch[j], 1, MPI_INT, j, teamComm[teamId]);
				MPI_Barrier(teamComm[teamId]);
			}
			ballChaserId = getBallChaserIdInTeam(expectedRoundToCatch);
			if (rankInTeam == ballChaserId) {
				int xNew, yNew;
				moveToBall(players[teamId][rankInTeam], ball, maxChasableSteps, &xNew, &yNew);
				players[teamId][rankInTeam][X] = xNew; players[teamId][rankInTeam][Y] = yNew;
			}
		}
	}

	
	MPI_Finalize();
	
	return 0;
}