#ifndef UTIL_H
#define UTIL_H

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "logging.h"

typedef struct {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
} Time;

double calculateCommTime(BaseAlert receivedAlert);
int getRankFromCoord(int x, int y, int numRows, int numCols, MPI_Comm nodeCartComm);
int isPresent(int array[], int len, int value);
Time getCurrentTime();

#endif
