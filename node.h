#ifndef NODE_H
#define NODE_H

#include "logging.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <mpi.h>
#include <omp.h>
#include <time.h>
#include <unistd.h>
#include <stddef.h>
 


#define CYCLE 5 //Time in seconds between each cycle
#define SHIFT_ROW 0 
#define SHIFT_COL 1 
#define DISP 1 //Amount to shift by
#define BASE_STATION 0
#define MAX_BASE_ALERT_LOGS 15 //Basically number of base alerts recoded before termination
#define PREDEFINED_WAIT 10 //In seconds, predfined period to wait in base station from neighbors of quadrant
 
//MPI Tags
#define ORDER_66 66 //Termination signal tag, hehe star wars reference
#define REQUEST_PORTS_AVAIL 126 //Request to ask if neighbour is occupied 
#define PORTS_AVAIL 200 //Response to other nodes on how many ports available
#define NODE_ALERT 69 //Alert sent to base station indicating node and its quadrant is full


int* checkAndProcessNearby(int* nearbyAvailable, int* nearbyCount, int* freeNodesCount, MPI_Comm world_comm);
void getNearbyNodes(int totalSize, int neighbors[MAX_NEIGHBORS], MPI_Comm nodeCartComm, int nearbyAvailable[MAX_NEARBY_EV], int *count, int numRows, int numCols, int checkingRank);
int evNodeIo(MPI_Comm world_comm, MPI_Comm comm, MPI_Datatype baseAlertType, int rows, int cols, int baseStationRank);

#endif