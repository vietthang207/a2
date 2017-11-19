#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define WIDTH 96
#define LENGTH 128
#define GOAL_LOW_Y 43
#define GOAL_HIGH_Y 51
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
#define NUM_ROUND_PER_HALF 2700
#define X 0
#define Y 1
#define SPEED 0
#define DRIBBING 1
#define KICK 2
#define INF 1000000
#define FIRST_HALF 0
#define SECOND_HALF 1
#define TEAM_ONE 0
#define TEAM_TWO 1

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

// Initialize attribute of a player
void initiateAttribute(int attribute[NUM_ATTRIBUTE]) {
	attribute[SPEED] = MIN_ATTRIBUTE + randomInt(MAX_ATTRIBUTE - MIN_ATTRIBUTE);
	int maxDribbing = minOf(MAX_ATTRIBUTE, TOTAL_ATTRIBUTE - attribute[SPEED] - MIN_ATTRIBUTE);
	attribute[DRIBBING] = MIN_ATTRIBUTE + randomInt(maxDribbing - MIN_ATTRIBUTE);
	attribute[KICK] = TOTAL_ATTRIBUTE - attribute[SPEED] - attribute[DRIBBING];
}

// Return 1 if the ball is inside patch row, col
int isInsidePatch(int row, int col, int ball[2]) {
	int minX = col * PATCH_SIZE, minY = row * PATCH_SIZE;
	int maxX = minX + PATCH_SIZE - 1, maxY = minY + PATCH_SIZE - 1;
	return (ball[X] >= minX) && (ball[X] <= maxX) && (ball[Y] >= minY) && (ball[Y] <= maxY);
}

// Get the process Id of the field patch where coor belongs to
int getPatch(int coor[2]) {
	int x = coor[X], y = coor[Y];
	int r = y / PATCH_SIZE, c = x / PATCH_SIZE;
	return r * GRID_LENGTH + c;
}

// Get maximum distance a player can run to chase the ball given that he runs toward it
// He can run 2*speed, but not more than 10 meters
int maxChasableDistance(int speed) {
	if (2 * speed < MAX_STEP) return 2 * speed;
	return MAX_STEP;
}

int getPlayerProcessId(int teamId, int rankInTeam) {
	return GRID_LENGTH * GRID_WIDTH + teamId * NUM_PLAYER_PER_TEAM + rankInTeam;
}

// Find the player who can chase the ball in the in the lease number of round
// Return rank in team of that player
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

int getBallChallenge(int dribbingSkill) {
	int r = 1 + randomInt(9);
	return r * dribbingSkill;
}

/** 
 * coor: coordinate of the player
 * ball: coordinate of the ball
 * The player will try to reach the ball in maxChasableDistance steps
 * if he is unable to reach the ball, then he run toward the ball.
 * He always run horizontally first, then vertically.
 * The player's new coordinate will be write to xNew and yNew
 * The number of steps player run will be write to steps
 * Return 1 if the player can reach the ball, return 0 otherwise
 */
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

// Return the rank of the process that represent the ball winner.
int chooseBallWinner(int numContesters, int ball[2], int *xBuf, int *yBuf, int *ballChallengeBuf, int *rankBuffer) {
	if (numContesters == 0) return -1;
	int numMax = 0, maxi = -INF;
	int tieBreak[NUM_PLAYER_PER_TEAM * NUM_TEAM];
	for (int i=1; i<=numContesters; i++) {
		if (xBuf[i]!=ball[X] || yBuf[i]!=ball[Y]) continue;
		if (ballChallengeBuf[i] > maxi) {
			maxi = ballChallengeBuf[i];
			tieBreak[0] = i;
			numMax = 1;
		} else if (ballChallengeBuf[i] == maxi) {
			tieBreak[numMax] = i;
			numMax ++;
		}
	}
	if (numMax < 1) return -1;
	int r = randomInt(numMax);
	return rankBuffer[tieBreak[r]];
}

/**
 * After win the ball, players always shoot toward the goal.
 * The location of the goal is determined by halfNo (first or second half) and teamId (Team A or team B)
 * He will shoot horizontally first, and then vertically
 * New location of the ball wil be recorded in xTarget and yTarget
 */

int shoot(int halfNo, int teamId, int x, int y, int kick, int *xTarget, int *yTarget) {
	*xTarget = ((halfNo==FIRST_HALF && teamId==TEAM_ONE) || (halfNo==SECOND_HALF && teamId==TEAM_TWO)) ? 0 : (LENGTH- 1);
	if (y >= GOAL_LOW_Y && y <= GOAL_HIGH_Y) {
		*yTarget = y;
	} else if (y < GOAL_LOW_Y) {
		*yTarget = GOAL_LOW_Y;
	} else {
		*yTarget = GOAL_HIGH_Y;
	}
	int dist = calDistance(x, y, *xTarget, *yTarget);
	int maxKick = kick * 2;
	if (maxKick >= dist) {
		return 1;
	} 
	int xDist = abs(*xTarget - x);
	int yDist = abs(*yTarget - y);
	if (xDist > maxKick) {
		if (*xTarget > x) {
			*xTarget = x + maxKick;
		} else {
			*xTarget = x - maxKick;
		}
		*yTarget = y;
		return 0;
	} else {
		maxKick -= xDist;
		if (*yTarget > y) {
			*yTarget = y + maxKick;
		} else {
			*yTarget = y - maxKick;
		}
		return 0;
	}
}

// Return the id of the scoring team
// Return -1 if no goal is scored
int getScoreTeam(int halfNo, int xBall, int yBall) {
	if (yBall < GOAL_LOW_Y || yBall > GOAL_HIGH_Y) return -1;
	if (xBall == 0 && halfNo == FIRST_HALF) return TEAM_ONE;
	if (xBall == LENGTH-1 && halfNo == SECOND_HALF) return TEAM_ONE;
	if (xBall == 0 && halfNo == SECOND_HALF) return TEAM_TWO;
	if (xBall == LENGTH-1 && halfNo == FIRST_HALF) return TEAM_TWO;
	return -1;
}

int main(int argc,char *argv[]) {
	int numtasks, rank;
	int ball[2], players[NUM_TEAM][NUM_PLAYER_PER_TEAM][2], expectedRoundToCatch[NUM_PLAYER_PER_TEAM], ballChallenge[NUM_TEAM][NUM_PLAYER_PER_TEAM];
	int oldBall[2], oldPlayers[NUM_TEAM][NUM_PLAYER_PER_TEAM][2];
	int isFieldProcess = -1, teamId = -1, rankInTeam = -1, row = -1, col = -1;
	int attribute[NUM_ATTRIBUTE], distToBall, maxChasableSteps, ballChaserId, color, rankInColoredComm, reached;
	int xBuf[NUM_PLAYER_PER_TEAM * NUM_TEAM + 1], yBuf[NUM_PLAYER_PER_TEAM * NUM_TEAM + 1];
	int ballChallengeBuf[NUM_PLAYER_PER_TEAM * NUM_TEAM + 1], rankBuffer[NUM_PLAYER_PER_TEAM * NUM_TEAM + 1], ballWinnerBuff[1];
	int halfNo, score[2];

	MPI_Init(&argc,&argv);
	MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	srand(rank * time(NULL));

	isFieldProcess = rank < GRID_WIDTH * GRID_LENGTH;
	if (isFieldProcess) {
		row = rank / GRID_LENGTH;
		col = rank % GRID_LENGTH;
		color = rank;
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
	// worldGroup: group of all processes
	// fieldGroup: group of all field processes
	// teamGroup: group of players from the same team
	MPI_Group worldGroup, fieldGroup , teamGroup[NUM_TEAM];
	MPI_Comm fieldComm, teamComm[NUM_TEAM], coloredComm;
	MPI_Comm_group(MPI_COMM_WORLD, &worldGroup);
	MPI_Group_incl(worldGroup, GRID_WIDTH * GRID_LENGTH, fieldRanks, &fieldGroup);
	MPI_Comm_create(MPI_COMM_WORLD, fieldGroup, &fieldComm);
	for (int i=0; i<NUM_TEAM; i++) {
		MPI_Group_incl(worldGroup, NUM_PLAYER_PER_TEAM, teamRanks[i], &teamGroup[i]);
		MPI_Comm_create(MPI_COMM_WORLD, teamGroup[i], &teamComm[i]);
	}

	// Initiate ball position 
	if (isFieldProcess) {
		ball[X] = 1 + randomInt(LENGTH - 2); ball[Y] = randomInt(WIDTH);
		oldBall[X] = ball[X]; oldBall[Y] = ball[Y];
		score[0] = 0; score[1] = 0;
	} else {
		initiateAttribute(attribute);
		maxChasableSteps = maxChasableDistance(attribute[SPEED]);
		players[teamId][rankInTeam][X] = randomInt(LENGTH);
		players[teamId][rankInTeam][Y] = randomInt(WIDTH);
		
	}

	for (int i=0; i<NUM_ROUND_PER_HALF * 2; i++) {
		halfNo = (i < NUM_ROUND_PER_HALF) ? 0 : 1;
		MPI_Bcast(ball, 2, MPI_INT, 0, MPI_COMM_WORLD);
		MPI_Barrier(MPI_COMM_WORLD);
		color = rank;		
		if (!isFieldProcess) {
			ballChallenge[teamId][rankInTeam] = -1;
			distToBall = calDistance(ball[X], ball[Y], players[teamId][rankInTeam][X], players[teamId][rankInTeam][Y]);
			expectedRoundToCatch[rankInTeam] = distToBall / maxChasableSteps;
			if (distToBall % maxChasableSteps != 0) expectedRoundToCatch[rankInTeam] ++;
			for (int j=0; j<NUM_PLAYER_PER_TEAM; j++) {
				MPI_Bcast(&expectedRoundToCatch[j], 1, MPI_INT, j, teamComm[teamId]);
				MPI_Barrier(teamComm[teamId]);
			}
			ballChaserId = getBallChaserIdInTeam(expectedRoundToCatch);
			reached = 0;
			if (rankInTeam == ballChaserId) {
				int xNew, yNew;
				reached = moveToBall(players[teamId][rankInTeam], ball, maxChasableSteps, &xNew, &yNew);
				players[teamId][rankInTeam][X] = xNew; players[teamId][rankInTeam][Y] = yNew;
				ballChallenge[teamId][rankInTeam] = reached ? getBallChallenge(attribute[DRIBBING]) : -1;
			}
			color = getPatch(players[teamId][rankInTeam]);
		}
		MPI_Comm_split(MPI_COMM_WORLD, color, rank, &coloredComm);
		MPI_Barrier(MPI_COMM_WORLD);
		MPI_Gather(&players[teamId][rankInTeam][X], 1, MPI_INT, xBuf, 1, MPI_INT, 0, coloredComm);
		MPI_Barrier(MPI_COMM_WORLD);
		MPI_Gather(&players[teamId][rankInTeam][Y], 1, MPI_INT, yBuf, 1, MPI_INT, 0, coloredComm);
		MPI_Barrier(MPI_COMM_WORLD);
		MPI_Gather(&ballChallenge[teamId][rankInTeam], 1, MPI_INT, ballChallengeBuf, 1, MPI_INT, 0, coloredComm);
		MPI_Barrier(MPI_COMM_WORLD);
		MPI_Gather(&rank, 1, MPI_INT, rankBuffer, 1, MPI_INT, 0, coloredComm);
		MPI_Barrier(MPI_COMM_WORLD);
		if (rank == getPatch(ball)) {
			int numContesters;
			MPI_Comm_size(coloredComm, &numContesters);
			numContesters --;
			ballWinnerBuff[0] = chooseBallWinner(numContesters, ball, xBuf, yBuf, ballChallengeBuf, rankBuffer);
		}
		MPI_Bcast(ballWinnerBuff, 1, MPI_INT, getPatch(ball), MPI_COMM_WORLD);
		MPI_Barrier(MPI_COMM_WORLD);
		if (rank == ballWinnerBuff[0]) {
			// kick the ball
			int xNew, yNew;
			shoot(halfNo, teamId, ball[X], ball[Y], attribute[KICK], &xNew, &yNew);
			ball[X] = xNew; ball[Y] = yNew;
		}
		if (ballWinnerBuff[0] != -1) {
			MPI_Bcast(ball, 2, MPI_INT, ballWinnerBuff[0], MPI_COMM_WORLD);
			MPI_Barrier(MPI_COMM_WORLD);
		}
		MPI_Comm_free(&coloredComm);

		if (rank == 0 || rank >= GRID_LENGTH * GRID_WIDTH) {
			color = 0;
		} else {
			color = 1;
		}
		MPI_Comm_split(MPI_COMM_WORLD, color, rank, &coloredComm);
		MPI_Barrier(MPI_COMM_WORLD);
		if (color == 0) {
			int tmpX = 0, tmpY = 0, tmpBc = -1;
			if (rank != 0) {
				tmpX = players[teamId][rankInTeam][X];
				tmpY = players[teamId][rankInTeam][Y];
				tmpBc = ballChallenge[teamId][rankInTeam];
			}
			MPI_Gather(&tmpX, 1, MPI_INT, xBuf, 1, MPI_INT, 0, coloredComm);
			MPI_Gather(&tmpY, 1, MPI_INT, yBuf, 1, MPI_INT, 0, coloredComm);
			MPI_Gather(&tmpBc, 1, MPI_INT, ballChallengeBuf, 1, MPI_INT, 0, coloredComm);
		}
		MPI_Comm_free(&coloredComm);

		if (rank == 0) {
			for (int j=0; j<NUM_TEAM; j++) {
				for (int k=0; k<NUM_PLAYER_PER_TEAM; k++) {
					int index = 1 + j * NUM_PLAYER_PER_TEAM + k;
					oldPlayers[j][k][X] = players[j][k][X];
					oldPlayers[j][k][Y] = players[j][k][Y];
					players[j][k][X] = xBuf[index];
					players[j][k][Y] = yBuf[index];
					ballChallenge[j][k] = ballChallengeBuf[index];
				}
			}
			printf("Round %d\n", i);
			printf("Ball is in %d %d\n", ball[X], ball[Y]);
			printf("%d win the ball\n", ballWinnerBuff[0]);
			
			for (int j=0; j<NUM_TEAM; j++) {
				printf("Team %d:\n", j + 1);
				for (int k=0; k<NUM_PLAYER_PER_TEAM; k++) {
					printf("%2d, old x: %3d, old y: %2d, ", k, oldPlayers[j][k][X], oldPlayers[j][k][Y]);
					printf("final x: %3d, final y: %2d, ", players[j][k][X], players[j][k][Y]);
					reached = (oldBall[X]==players[j][k][X] && oldBall[Y]==players[j][k][Y]);
					int kicked = (getPlayerProcessId(j, k) == ballWinnerBuff[0]);
					printf("reached %d, kicked %d, bc %4d\n", reached, kicked, ballChallenge[j][k]);
				}
			}
			int scoreTeam = getScoreTeam(halfNo, ball[X], ball[Y]);
			if (scoreTeam != -1) {
				score[scoreTeam] ++;
				if (scoreTeam==TEAM_ONE) printf("GOAL GOAL GOAL GOAL GOAL GOAL GOAL Team A score!!!\n");
				else printf("GOAL GOAL GOAL GOAL GOAL GOAL GOAL Team B score!!!\n");
				ball[X] = 1 + randomInt(LENGTH - 2); ball[Y] = randomInt(WIDTH);
			}
			printf("Score: %d - %d\n", score[0], score[1]);
		}
		oldBall[X] = ball[X]; oldBall[Y] = ball[Y];
		
	}

	
	MPI_Finalize();
	
	return 0;
}