#include <cmsis_os2.h>
#include "general.h"

// add any #includes here
#include <string.h>

// add any #defines here
#define MAX_GENERALS 7
#define MAX_TRAITORS ((MAX_GENERALS - 1) / 3) // e.g. 7&2, 6&1, 5&1, 4&1

// add global variables here
uint8_t m; // m is the number of traitors
uint8_t n; // n is the number of generals
uint8_t reporterID;
uint8_t commanderID;
uint8_t traitor1ID;
uint8_t traitor2ID;
uint8_t barrierIdx;
char commandChar;
osSemaphoreId_t semaphoreA;
osSemaphoreId_t barriers[MAX_GENERALS - 1];
osMutexId_t mutexA;
osMutexId_t mutexB;

void send_oral_message(uint8_t mLevel, char *msg, uint8_t receiverID);

/** msgQueueMatrix has osMessageQueue instances as its elements.
	* Max num of rows = max num of recursion levels = (2+1) = (m+1)
	* Max num of columns = max num of lieutenants = (7-1) = (n-1)
	*/
osMessageQueueId_t msgQueueMatrix[MAX_TRAITORS + 1][MAX_GENERALS - 1];

/** Record parameters and set up any OS and other resources
  * needed by your general() and broadcast() functions.
  * nGeneral: number of generals
  * loyal: array representing loyalty of corresponding generals
  * reporter: general that will generate output
  * return true if setup successful and n > 3*m, false otherwise
  */
bool setup(uint8_t nGeneral, bool loyal[], uint8_t reporter)
{

	// 1. Calculate the number of traitors:
	uint8_t numTraitors = 0;
	traitor1ID = 7;
	traitor2ID = 8;
	for (int i = 0; i < nGeneral; i++)
	{
		if (loyal[i] == false)
		{
			if (traitor1ID == 7)
			{
				traitor1ID = i;
			}
			else
			{
				traitor2ID = i;
			}
			numTraitors++;
		}
	}
	// Check if (n > 3*m) using c_assert():
	m = numTraitors;
	n = nGeneral;
	if (c_assert(n > 3 * m) == false)
	{
		return false;
	};

	// 2. Initialize other variables:
	reporterID = reporter;
	semaphoreA = osSemaphoreNew(1, 0, NULL);
	for (int i = 0; i < (n - 1); i++)
	{
		barriers[i] = osSemaphoreNew(1, 0, NULL);
	}
	barrierIdx = 0;
	mutexA = osMutexNew(NULL);
	mutexB = osMutexNew(NULL);
	// msgQueueMatrix should be an [(m+1)*n] matrix
	for (int i = 0; i < (m + 1); i++)
	{ // Levels of recursion: 1 + OM(m)~OM(1)
		for (int j = 0; j < n; j++)
		{ // Lieutenants + Commander (used as a placeholder)
			// TODO: Optimize maxNumMsgs to save momory (num of msgs: 1, 5, 20)
			uint8_t maxNumMsgs = (MAX_GENERALS - 2) * (MAX_GENERALS - 3); // Max num of msg's each lieutenant will receive
			//
			if (i == 0)
			{
				maxNumMsgs = 1;
			}
			else if (i == 1)
			{
				maxNumMsgs = 5;
			}
			else
			{
				maxNumMsgs = 20;
			}
			//
			uint8_t numChars = (i + 1) * 2 + 2; // e.g. Recursion #1 (i=1): 6 chars for "2:1:A", inluding NULL char
			msgQueueMatrix[i][j] = osMessageQueueNew(maxNumMsgs, numChars * sizeof(char), NULL);
		}
	}

	return true;
}

/** Delete any OS resources created by setup() and free any memory
  * dynamically allocated by setup().
  */
void cleanup(void)
{
	osMutexAcquire(mutexA, osWaitForever);
	printf("\n");
	osMutexRelease(mutexA);
	// Delete semaphores & mutexes:
	osSemaphoreDelete(semaphoreA);
	for (int i = 0; i < MAX_GENERALS - 1; i++)
	{
		osSemaphoreDelete(barriers[i]);
	}
	osMutexDelete(mutexA);
	// Delete msg queues:
	for (int i = 0; i < MAX_TRAITORS + 1; i++)
	{
		for (int j = 0; j < MAX_GENERALS - 1; j++)
		{
			osMessageQueueDelete(msgQueueMatrix[i][j]);
		}
	}
}

/** This function performs the initial broadcast to n-1 generals.
  * It should wait for the generals to finish before returning.
  * Note that the general sending the command does not participate
  * in the OM algorithm.
  * command: either 'A' or 'R'
  * sender: general sending the command to other n-1 generals
  */
void broadcast(char command, uint8_t commander)
{
	// 1. Broadcasts the inital message from General 0:
	commanderID = commander;
	commandChar = command;
	// osMutexAcquire(mutexA, osWaitForever);
	// printf("[m: %d, n: %d, commanderID: %d, reporterID: %d, traitor1ID: %d, traitor2ID: %d]\n",
	// m, n, commanderID, reporterID, traitor1ID, traitor2ID);
	// osMutexRelease(mutexA);
	// Broadcast: "<commanderID>:<command>"
	for (int i = 0; i < n; i++)
	{
		if (i != commanderID)
		{
			char initialCommand[] = "x:x";
			initialCommand[0] = commanderID + '0';
			if (commanderID != traitor1ID && commanderID != traitor2ID)
			{ // (if loyal commander)
				initialCommand[2] = commandChar;
			}
			else
			{ // (if treacherous commander)
				if (i % 2 == 0)
				{
					initialCommand[2] = 'R';
				}
				else
				{
					initialCommand[2] = 'A';
				}
			}
			// osMutexAcquire(mutexA, osWaitForever);
			// printf("[initialCommand: %s]\n", initialCommand);
			// osMutexRelease(mutexA);
			osMessageQueuePut(msgQueueMatrix[0][i], initialCommand, 0, 0);
		}
	}
	// osMutexAcquire(mutexA, osWaitForever);
	// printf("[BP1]\n");
	// osMutexRelease(mutexA);

	// 2. Returns "AFTER the algorithm completes and the output is printed":
	osSemaphoreAcquire(semaphoreA, osWaitForever); // Blocks broadcast()

	// osMutexAcquire(mutexA, osWaitForever);
	// printf("broadcast() ends.\n");
	// osMutexRelease(mutexA);
}

/** Generals are created before each test and deleted after each
  * test.  The function should wait for a value from broadcast()
  * and then use the OM algorithm to solve the Byzantine General's
  * Problem.  The general designated as reporter in setup()
  * should output the messages received in OM(0).
  * idPtr: pointer to general's id number which is in [0,n-1]
  */
void general(void *idPtr)
{
	uint8_t id = *(uint8_t *)idPtr;

	// Call send_oral_message() for this general:
	if (id != commanderID)
	{
		char initialCmd[4];
		char commandOM1[] = "x:"; // TODO: May need to expand the size to use strcat() successfully.
		osMessageQueueGet(msgQueueMatrix[0][id], initialCmd, NULL, osWaitForever);
		commandOM1[0] = id + '0';
		strcat(commandOM1, initialCmd);
		osMutexAcquire(mutexA, osWaitForever);
		// printf("[commandOM1: %s, initialCmd: %s]\n", commandOM1, initialCmd);
		osMutexRelease(mutexA);
		for (int i = 0; i < n; i++)
		{
			// Send oral message:
			if (i != commanderID && i != id)
			{
				send_oral_message(m, commandOM1, i);
			}
		}

		// Use barriers to sync all the lieutenants:
		bool lastBarrier = false;

		osMutexAcquire(mutexB, osWaitForever);
		uint8_t barrierIdxCopy = barrierIdx;
		osMutexRelease(mutexB);

		if (barrierIdxCopy == (n - 1) - 1)
		{ // If the last barrier, unblock the first barrier:
			lastBarrier = true;
			osSemaphoreRelease(barriers[0]);
		}

		osMutexAcquire(mutexB, osWaitForever);
		barrierIdx++;
		osMutexRelease(mutexB);

		osSemaphoreAcquire(barriers[barrierIdxCopy], osWaitForever);
		if (!lastBarrier)
		{ // If not the last barrier, unblock the next barrier:
			osSemaphoreRelease(barriers[barrierIdxCopy + 1]);
		}
		else
		{									// If the last barrier, signal broadcast() to continue:
			osSemaphoreRelease(semaphoreA); // Signals broadcast() to continue
		}
	}
}

/** This function is the helper function that implements the OM algorithm.
	* mLevel: decrements from the number of traiters, OM(m), down to 0, OM(0)
	* msg: e.g. "1:2:0:A" for "Attack"
	* receiverID: the ID of the general that receives <msg>
	*/
void send_oral_message(uint8_t mLevel, char *msg, uint8_t receiverID)
{

	// osMutexAcquire(mutexA, osWaitForever);
	// printf("[mLevel: %d, msg: %s, receiverID: %d]\n", mLevel, msg, receiverID);
	// osMutexRelease(mutexA);

	// 1. The actual OM(): Update the msg queue matrix:
	osMessageQueuePut(msgQueueMatrix[mLevel][receiverID], msg, 0, 0);
	// Note: CURRENT level of recursion is already done here!

	// 2. Prepare for the NEXT recursion:
	if (mLevel == 1)
	{
		// NEXT recursion will be OM(0) - printing out
		if (receiverID == reporterID)
		{
			if (msg[0] - '0' == traitor1ID || msg[0] - '0' == traitor2ID)
			{
				// Pass incorrect msg:
				for (int j = 0; j < 7; j++)
				{
					if (msg[j] == 'A' || msg[j] == 'R')
					{
						if ((msg[0] - '0') % 2 == 0)
						{
							msg[j] = 'R';
						}
						else
						{
							msg[j] = 'A';
						}
						break;
					}
				}
			}
			osMutexAcquire(mutexA, osWaitForever);
			printf(" %s", msg);
			osMutexRelease(mutexA);
		}
	}
	else
	{
		// NEXT recursion call needs to be made:
		for (int i = 0; i < n; i++)
		{
			if (i != commanderID && i != receiverID)
			{
				char updatedMsg[] = "x:";
				updatedMsg[0] = receiverID + '0';
				strcat(updatedMsg, msg);
				// If treacherous lieutenant:
				//				osMutexAcquire(mutexA, osWaitForever);
				//				printf("[traitor1ID %d, traitor2ID %d, i %d]\n", traitor1ID, traitor2ID, i);
				//				osMutexRelease(mutexA);
				if (receiverID == traitor1ID || receiverID == traitor2ID)
				{
					// Replace
					for (int j = 0; j < 7; j++)
					{
						if (updatedMsg[j] == 'A' || updatedMsg[j] == 'R')
						{
							if (receiverID % 2 == 0)
							{
								updatedMsg[j] = 'R';
							}
							else
							{
								updatedMsg[j] = 'A';
							}
							break;
						}
					}
				}
				// osMutexAcquire(mutexA, osWaitForever);
				// printf("[mLevel-1: %d, updatedMsg: %s, i: %d]\n", mLevel-1, updatedMsg, i);
				// osMutexRelease(mutexA);
				send_oral_message(mLevel - 1, updatedMsg, i);
			}
		}
	}
}
