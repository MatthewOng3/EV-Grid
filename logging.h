#ifndef LOGGING_H
#define LOGGING_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <omp.h>
#include <time.h>

#define MAX_NEIGHBORS 4
#define MAX_NEARBY_EV 8
#define PORTS 12 //Number of ports per node
#define PORTS_THRESHOLD 6


// Type definitions
typedef struct {
    int x;
    int y;
} CoordTuple;

/*Struct for the information sent to the base station when node and it's quadrants are full*/
typedef struct {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    int rank; // The rank of the node reporting the alert
    int numMsgsExchanged; // Number of messages exchanged with neighbors
    int neighbors[MAX_NEIGHBORS]; //Store ranks of all neighbors around
    int neighborPortsAvail[MAX_NEIGHBORS]; //Store the number of ports available  of all neighbor nodes
    int portsAvailable; //Number of ports available in the sending node
    double commTimeBetweenNodes;
    int numOfNeighbors;
    int nearbyNodes[MAX_NEARBY_EV];
    int nearbyNodesCount;
    double startCommTime;
} BaseAlert;

// Function prototypes
BaseAlert createAlertReport(int rank, int portsAvailable, int neighbors[MAX_NEIGHBORS], int neighborPortsAvail[MAX_NEIGHBORS], int numOfNeighbors, double commTime, int numOfMsg, int nearbyAvailable[MAX_NEARBY_EV], int nearbyCount);
void logBaseAlertToFile(BaseAlert alert, int iteration, CoordTuple* coordsArray, int nearbyAvailable[MAX_NEARBY_EV], int nearbyCount, int* freeNodes, int freeNodesCount, double baseCommTime, double commTime);

#endif  