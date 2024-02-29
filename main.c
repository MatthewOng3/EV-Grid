#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <omp.h>
#include <time.h>
#include <stdbool.h>
#include <unistd.h>
#include <stddef.h>
 
#include "node.h"


#define SHIFT_ROW 0 
#define SHIFT_COL 1 
#define DISP 1 //Amount to shift by
#define BASE_DURATION 120

//MPI Tags
#define ORDER_66 66 //Termination signal tag, hehe star wars reference
#define REQUEST_PORTS_AVAIL 126 //Request to ask if neighbour is occupied 
#define PORTS_AVAIL 200 //Response to other nodes on how many ports available
#define NODE_ALERT 69 //Alert sent to base station indicating node and its quadrant is full
#define BASE_STATION_RESPONSE 123 //Response from base station about nearby available nodes

MPI_Datatype createMPIBaseAlertType();
int baseIo(MPI_Comm world_comm, MPI_Comm commm, MPI_Datatype baseAlertType, int baseStationRank, int numCols);
void sendAlertResponse(int nearbyAvailCount, int nearbyAvail[MAX_NEARBY_EV], int destinationRank, MPI_Comm worldComm );


/*------------MAIN FUNCTION------------*/
int main(int argc, char **argv) {
    
    int rank, size, cartRank;
    int rows, cols; // Dimensions of the grid
    
    //Init multi threaded 
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);

    //Returns size of group of machines/tasks that the current task is involved with 
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    //Master rank prompt for user rows and cols
    if (rank == size - 1){
        // Ensure there are enough arguments (2 additional arguments for rows and cols)
        if (argc < 3) {
 
            MPI_Abort(MPI_COMM_WORLD, 1); // Abort if the number of arguments is incorrect
        }

        // Convert command line arguments to integer values
        rows = atoi(argv[1]);
        cols = atoi(argv[2]);
    }
    
    //Broadcast the values of rows and cols to all other processers
    MPI_Bcast(&rows, 1, MPI_INT, size - 1, MPI_COMM_WORLD);
    MPI_Bcast(&cols, 1, MPI_INT, size - 1, MPI_COMM_WORLD);

    //Seed random number generator
    srand(time(NULL) + rank);

    /*------------Split comm------------*/
    MPI_Comm splitComm; 
    MPI_Comm_split(MPI_COMM_WORLD, rank == size-1, 0, &splitComm);
    
    //Create MPI base station alert type
    MPI_Datatype baseAlertType = createMPIBaseAlertType();
    
    if (rank == size - 1){
        baseIo(MPI_COMM_WORLD, splitComm, baseAlertType, size - 1, cols);
    }
    else{
        evNodeIo(MPI_COMM_WORLD, splitComm, baseAlertType, rows, cols, size - 1);
    }
    
    //Free custom datatype and cart communicator
    MPI_Type_free(&baseAlertType);
    MPI_Finalize();
    return 0;
}

/*------------BASE STATION LOGIC------------*/
int baseIo(MPI_Comm world_comm, MPI_Comm comm, MPI_Datatype baseAlertType, int baseStationRank, int numCols){

    int worldSize, nodesSize;
    bool terminationSignal = false;

    /*------------Collect and store coordinates of ranks using process ranks as indexes------------*/
    MPI_Comm_size(world_comm, &worldSize);
    nodesSize = worldSize - 1;

    //Base station gathers all the ranks and their corresponding cart coords for logging in the future
    CoordTuple* coordsArray = (CoordTuple* ) malloc(nodesSize * sizeof(CoordTuple));
    int* tempArray = (int* ) malloc((2 * worldSize)* sizeof(int)); //Add 2 to ignore the base station buffer

    //Gather the rank coords of node processes
    int rankCoords[2] = {-1, -1};
    MPI_Gather(rankCoords, 2, MPI_INT, tempArray, 2, MPI_INT, baseStationRank, world_comm);


    //Insert tempArray to the coordsArray converting to the struct as we go
    for(int i = 0; i < worldSize - 1; i++){
        CoordTuple newCoord;
        coordsArray[i].x = tempArray[2*i];
        coordsArray[i].y = tempArray[2*i + 1];
    } 
    
    double baseLoopStart = MPI_Wtime();
    double totalBaseCommTime = 0;
    MPI_Status status;
    int logCount = 0;

    omp_set_num_threads(2);
    
    #pragma omp parallel sections
    {   
        /*Thread to listen for incoming alerts from occupied node and its quadrant*/
        #pragma omp section
        {   
             
            //Keep listening until 
            while(MPI_Wtime() - baseLoopStart < BASE_DURATION){
                sleep(2);
                int message_available;
                MPI_Status probeStatus;

                MPI_Iprobe(MPI_ANY_SOURCE, NODE_ALERT, world_comm, &message_available, &probeStatus);
                
                if(message_available) {
                    double  updatedCommTime = 0;
 
                    //Receive the alert message
                    BaseAlert receivedAlert;
                    MPI_Recv(&receivedAlert, 1, baseAlertType, MPI_ANY_SOURCE, NODE_ALERT, world_comm, &status);
                    
                    double baseCommTime =  calculateCommTime(receivedAlert);//MPI_Wtime() - receivedAlert.startCommTime;
                    totalBaseCommTime += baseCommTime;
                    int nearbyCount = receivedAlert.nearbyNodesCount;
                    
                    /*-------------Check nearby nodes for availability other than the neighbors--------------*/
                    int freeNodesCount;
                    int* freeNodes = checkAndProcessNearby(receivedAlert.nearbyNodes, &nearbyCount, &freeNodesCount, world_comm);
                    //Send back base station response of nearby available nodes
                    sendAlertResponse(nearbyCount, receivedAlert.nearbyNodes, receivedAlert.rank, world_comm);
                    
                    //Get total communication time
                    updatedCommTime = baseCommTime + receivedAlert.commTimeBetweenNodes; 
                    //Log everything to txt file
                    logBaseAlertToFile(receivedAlert, logCount, coordsArray, receivedAlert.nearbyNodes, nearbyCount, freeNodes, freeNodesCount, baseCommTime, updatedCommTime);
                    
                    //Update the log count
                    logCount ++;
                    
                    free(freeNodes);
                }
            }
        }
    }

    /*------------Send termination signal------------*/
    //Create request array, each index representing request sent to each node
    MPI_Request* requests = (MPI_Request*) malloc(nodesSize * sizeof(MPI_Request));
    terminationSignal = true;

    //Broadcast termination signal to all slave nodes
    for(int i = 0; i < nodesSize; i++) {
        MPI_Isend(&terminationSignal, 1, MPI_C_BOOL, i, ORDER_66, world_comm, &requests[i]);
    }

    //Wait for all MPI_Isend to complete before progressing
    MPI_Waitall(nodesSize, requests, MPI_STATUS_IGNORE);

    printf("\n\nTotal base comm time: %f\n\n", totalBaseCommTime);

    free(requests);
    free(tempArray);
    free(coordsArray);

    return 0;
}

/*Based on the nearby available nodes, if none send no nodes available back to the reporting node, else send the available nodes*/
void sendAlertResponse(int nearbyAvailCount, int nearbyAvail[MAX_NEARBY_EV], int destinationRank, MPI_Comm worldComm ){
    //Else send the available nodes
    if(nearbyAvailCount > 0){
        MPI_Request req;
        MPI_Isend(nearbyAvail, nearbyAvailCount, MPI_INT, destinationRank, BASE_STATION_RESPONSE, worldComm, &req);
        MPI_Request_free(&req);
    }
    //If no available nearby send to reporting node none available
    else{
        MPI_Request req;
        MPI_Isend(0,1, MPI_INT, destinationRank, BASE_STATION_RESPONSE, worldComm, &req);
        MPI_Request_free(&req);
    }
}

/*Create the MPI custom type based on BaseAlert struct, which is the info needed to log in base station from node*/
MPI_Datatype createMPIBaseAlertType() {

    MPI_Datatype MPI_BaseAlert_Type;
    
    //Indicates how many of each datatype are in the struct, everything is a single int
    int blocklengths[16] = {1, 1, 1, 1, 1, 1, 1, 1, MAX_NEIGHBORS, MAX_NEIGHBORS, 1, 1, 1, MAX_NEARBY_EV,1, 1};
    //Store byte offsets of each element in struct
    MPI_Aint displacements[16];
    //Datatype of each element in struct
    MPI_Datatype types[16] = {MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_DOUBLE, MPI_INT, MPI_INT, MPI_INT, MPI_DOUBLE};

    MPI_Aint start, temp;
    BaseAlert sample;

    displacements[0] = offsetof(BaseAlert, year);
    displacements[1] = offsetof(BaseAlert, month);
    displacements[2] = offsetof(BaseAlert, day);
    displacements[3] = offsetof(BaseAlert, hour);
    displacements[4] = offsetof(BaseAlert, minute);
    displacements[5] = offsetof(BaseAlert, second);
    displacements[6] = offsetof(BaseAlert, rank);
    displacements[7] = offsetof(BaseAlert, numMsgsExchanged);
    displacements[8] = offsetof(BaseAlert, neighbors);
    displacements[9] = offsetof(BaseAlert, neighborPortsAvail);
    displacements[10] = offsetof(BaseAlert, portsAvailable);
    displacements[11] = offsetof(BaseAlert, commTimeBetweenNodes);
    displacements[12] = offsetof(BaseAlert, numOfNeighbors);
    displacements[13] = offsetof(BaseAlert, nearbyNodes);
    displacements[14] = offsetof(BaseAlert, nearbyNodesCount);
    displacements[15] = offsetof(BaseAlert, startCommTime);
    

    //Create and commit datatype
    MPI_Type_create_struct(16, blocklengths, displacements, types, &MPI_BaseAlert_Type);
    MPI_Type_commit(&MPI_BaseAlert_Type);
     
    return MPI_BaseAlert_Type;
}
 


