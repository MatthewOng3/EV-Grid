#include "node.h"

/*------------EV NODE LOGIC------------*/
int evNodeIo(MPI_Comm world_comm, MPI_Comm comm, MPI_Datatype baseAlertType, int rows, int cols, int baseStationRank){
    omp_set_nested(1);
    omp_set_num_threads(2);
    int nodeSize, nodeRank, worldSize ;
    int totalMessages = 0;
    int status[PORTS]; //Stores the availabilities of each port
    
    bool terminationSignal = false;
    MPI_Comm_size(world_comm, &worldSize); // size of the world
    MPI_Comm_size(comm, &nodeSize); //
    MPI_Comm_rank(comm, &nodeRank); // rank of the slave communicator
    
    // Set up the Cartesian grid
    int dims[2] = {rows, cols}; //Specify number of processors each dimension
    int periods[2] = {0, 0}; //Non-periodic grid, does not wrap around

    MPI_Comm nodeComm2d; //Create new cart communicator
    MPI_Cart_create(comm, 2, dims, periods, 0, &nodeComm2d);
    
    //Get the cartesian coordinates of the current rank process
    int rankCoords[2];
    MPI_Cart_coords(nodeComm2d, nodeRank, 2, rankCoords);
    
    //Send rank coordinates to base station
    MPI_Gather(&rankCoords, 2, MPI_INT, NULL, 0, MPI_INT, baseStationRank, world_comm);

    //Listen for base station termination signal
    MPI_Request terminationRequest;
    MPI_Irecv(&terminationSignal, 1, MPI_C_BOOL, baseStationRank, ORDER_66, world_comm, &terminationRequest);

    int flag =  0; //Flag to check if termination signal is received
    int portsAvailable = 0; //Ports available for the current node

    // Find neighboring ranks using MPI_Cart_shift
    int up, down, left, right;
    MPI_Cart_shift(nodeComm2d, SHIFT_ROW, DISP, &up, &down);
    MPI_Cart_shift(nodeComm2d, SHIFT_COL, DISP, &left, &right);
    
    //Store the neighbours response from cart shift
    int neighbors[MAX_NEIGHBORS] = {up, down, left, right}; 
    int numOfNeighbours = 0;

    for(int i = 0; i < MAX_NEIGHBORS; i++){
        if(neighbors[i] >= 0 && neighbors[i] != MPI_PROC_NULL){
            //Basically track number of exchanged messages to send to base station
            numOfNeighbours += 1;
        }
    }

    //Compute the nearby neighbors to the current rank process quadrant
    int nearbyAvailable[MAX_NEARBY_EV];
    int nearbyCount;
    getNearbyNodes(nodeSize, neighbors, nodeComm2d, nearbyAvailable, &nearbyCount, rows, cols, nodeRank);
    
    double totalTimeTakenStart = MPI_Wtime();
    double commTimeBetweenNodes = 0;
    
    #pragma omp parallel sections
    {   
        #pragma omp section
        {
          
            while(!flag ){
                /*------------Listening for Base station termination------------*/
                //Test if current process terminationRequest has been completed
                MPI_Test(&terminationRequest, &flag, MPI_STATUS_IGNORE);
                
                // If terminationSignal has been received and value is set properly, then terminate node
                if(flag && terminationSignal) {
                    break;  // exit the while loop
                }

                //Simulate port availability on each thread
                #pragma omp parallel num_threads(PORTS) shared(status)
                {
                    int portNumber = omp_get_thread_num();

                    int randomAvailable = (rand() % 10) < 7 ? 1 : 0;
                    #pragma omp critical
                    {
                        status[portNumber] = randomAvailable;
                    }
                }
                //Delay for a cycle
                sleep(CYCLE);

                //Compute how many ports are occupied
                int occupied = 0;
                for(int i = 0; i < PORTS; i++){
                    if(status[i] == 1){
                        occupied += 1;
                    }
                }

                //Get the number of occupied ports of current node
                portsAvailable = PORTS - occupied;
                MPI_Status neighborSwapStatus;

                //If almost all ports are occupied, ask for neighbour availability
                if (occupied >= PORTS_THRESHOLD) {
                    
                    //Track time taken
                    // double start_time, elapsed_time;
                    MPI_Request sendRequest, recvRequest;

                    int completed;
                    int neighborPortsAvail[MAX_NEIGHBORS] = {-1, -1, -1, -1}; // -1 indicates no neighbor in that direction

                    MPI_Request sendRequests[MAX_NEIGHBORS];
                    MPI_Request recvRequests[MAX_NEIGHBORS];
                    int sendCount = 0, recvCount = 0;

                    //Measure the starting time for communication with nodes
                    double startCommTimeBetweenNodes = MPI_Wtime();

                    //Loop through neighbors and prompt for availability
                    for (int i = 0; i < MAX_NEIGHBORS; i++) {
                        if (neighbors[i] != MPI_PROC_NULL && neighbors[i] >= 0) {
                            // Initiate a non-blocking send to the neighbor
                            MPI_Isend(&nodeRank, 1, MPI_INT, neighbors[i], REQUEST_PORTS_AVAIL, nodeComm2d,  &sendRequests[sendCount++]);
                            // Initiate a non-blocking receive from the neighbor
                            MPI_Irecv(&neighborPortsAvail[i], 1, MPI_INT, neighbors[i], PORTS_AVAIL, nodeComm2d, &recvRequests[recvCount++]);

                        }
                    }

                    MPI_Waitall(sendCount, sendRequests, MPI_STATUSES_IGNORE);

                    double start_time = MPI_Wtime();
                    double elapsed_time = 0;

                    //Check if all request has been fulfilled with timeout mechanism
                    for (int i = 0; i < recvCount; i++) {
                        int completed = 0;

                        while (!completed) {
                            MPI_Test(&recvRequests[i], &completed, MPI_STATUS_IGNORE);
                            
                            elapsed_time = MPI_Wtime() - start_time;
                            if (elapsed_time > 10.0) {
                                break;
                            }
                        }
                        if (completed) {
                            //Track total number of exchanged message
                            totalMessages += 1;
                        }
                    }

                    
                    //Calculate total communication time
                    double currentCommTime = MPI_Wtime() - startCommTimeBetweenNodes;
                    commTimeBetweenNodes = commTimeBetweenNodes + currentCommTime;
                    
                    bool allOccupied = true; // Flag to check if all neighbors node's ports are also mostly occupied
                    // Check if neighbors have vacancies or if they're also heavily occupied
                    for (int i = 0; i < MAX_NEIGHBORS; i++) {
                        // If at any point the ports available is >= then the treshhold 
                        if (neighborPortsAvail[i] >= PORTS - PORTS_THRESHOLD) {
                            allOccupied = false;
                        } 
                    }
                    
                    //Send alert to base station if all neighbors are occupied
                    MPI_Request baseStationAlert;
                    
                    if (allOccupied) {
                        //Create and send alert report to base station
                        BaseAlert alertToSend = createAlertReport(nodeRank, portsAvailable, neighbors, neighborPortsAvail, numOfNeighbours, currentCommTime, numOfNeighbours, nearbyAvailable, nearbyCount);
                        MPI_Isend(&alertToSend, 1, baseAlertType, baseStationRank, NODE_ALERT, world_comm, &baseStationAlert); 
                    }
                    //Else print to console the available nodes and how many ports available
                    else{

                        printf("\n------------NODE %d HAS AVAILABILITY----------\n", nodeRank);
                        fflush(stdout);

                        for (int i = 0; i < MAX_NEIGHBORS; i++) {
                            // If at any point the ports available is >= then the treshhold 
                            if (neighborPortsAvail[i] >= PORTS - PORTS_THRESHOLD) {
                                printf("%d ports available at neighbor rank %d\n", neighborPortsAvail[i], neighbors[i]);
                                fflush(stdout);
                            } 
                        } 
            
                    }
                }
            }
        }
        /*------------Thread to listen for prompting for ports availability from adjacent nodes------------*/
        #pragma omp section
        {   
            while(!flag){
                
                int message_available;
                MPI_Status probeStatus;

                //Keep checking if any request is coming in
                MPI_Iprobe(MPI_ANY_SOURCE, REQUEST_PORTS_AVAIL, nodeComm2d, &message_available, &probeStatus);

                //Check for termination signal
                if(flag && terminationSignal) {
                    break;   
                }  
                
                if(message_available) {
                    int receivedRequestFrom; //Rank of node the request is received
                    MPI_Recv(&receivedRequestFrom, 1, MPI_INT, probeStatus.MPI_SOURCE, probeStatus.MPI_TAG, nodeComm2d, MPI_STATUS_IGNORE);
                     
                    fflush(stdout);
                    MPI_Ssend(&portsAvailable, 1, MPI_INT, receivedRequestFrom, PORTS_AVAIL, nodeComm2d);
                    
                    /*----------Check if termination signal has been received after each response*/
                    MPI_Test(&terminationRequest, &flag, MPI_STATUS_IGNORE);
                    
                    // If terminationSignal has been received and value is set properly, then terminate node
                    if(flag && terminationSignal) {
                        break;   
                    }   
                }
                
            }
        }
    }
    //For printing results
    int totalMessagesResult = 0;
    
    MPI_Reduce(&totalMessages, &totalMessagesResult, 1, MPI_INT, MPI_SUM, 0, nodeComm2d);

    
    if(nodeRank == 0){
        printf("\nTotal messages sent: %d\n", totalMessagesResult);
    }   
 
    double totalTimeAllProc = 0;
    MPI_Reduce(&commTimeBetweenNodes, &totalTimeAllProc, 1, MPI_DOUBLE, MPI_SUM, 0, nodeComm2d);

    
    if(nodeRank == 0){
        printf("\nTotal communication time between EV Nodes: %f\n", totalTimeAllProc);
    }   
  
    MPI_Comm_free(&nodeComm2d);
    return 0;
}

/*Check the available nodes and listen for alerts for each one and response accordingly*/
int* checkAndProcessNearby(int* nearbyAvailable, int* nearbyCount, int* freeNodesCount, MPI_Comm world_comm) {
    //
    int maxNearby = *nearbyCount;
    int reported[maxNearby];

    for (int i = 0; i < maxNearby; i++) {
        reported[i] = 0;
    }

    #pragma omp parallel for
    for (int i = 0; i < maxNearby; i++) {
        double start_time = MPI_Wtime();
        double elapsed_time = 0;

        //While no result present from MPI_Iprobe
        while (!reported[i]) {
            #pragma omp critical
            {
                MPI_Iprobe(nearbyAvailable[i], NODE_ALERT, world_comm, &reported[i], MPI_STATUS_IGNORE);
            }
            //If time elapsed past 20 seconds
            elapsed_time = MPI_Wtime() - start_time;
            if (elapsed_time > PREDEFINED_WAIT) {
                break;
            }
        }
    }

    // Prepare a new array for the nodes that haven't reported
    int* newNearbyAvailable = (int*) malloc(maxNearby * sizeof(int));
    int updatedCount = 0;

    //Create the new array 
    for (int i = 0; i < maxNearby; i++) {
        if (!reported[i]) {
            newNearbyAvailable[updatedCount] = nearbyAvailable[i];
            updatedCount++;
        }
    }

    *freeNodesCount = updatedCount;

    // Resize the new array
    newNearbyAvailable = (int*) realloc(newNearbyAvailable, updatedCount * sizeof(int));

    return newNearbyAvailable;

}

/*Retrieve the neighbor's neighbors of a rank*/
void getNearbyNodes(int totalSize, int neighbors[MAX_NEIGHBORS], MPI_Comm nodeCartComm, int nearbyAvailable[MAX_NEARBY_EV], int *count, int numRows, int numCols, int checkingRank) {
    int index = 0;

    for(int i = 0; i < MAX_NEIGHBORS; i++) {
        //Get all the neighboring ranks for each neighbor
        if(neighbors[i] != MPI_PROC_NULL && neighbors[i] >= 0) {
            int currentCoord[2];
            
            MPI_Cart_coords(nodeCartComm, neighbors[i], 2, currentCoord);

            int upRank, downRank, leftRank, rightRank;

            int inputCoords[2];
            upRank = getRankFromCoord(currentCoord[0] - 1, currentCoord[1], numRows, numCols, nodeCartComm);
            downRank = getRankFromCoord(currentCoord[0] + 1, currentCoord[1], numRows, numCols, nodeCartComm);
            leftRank = getRankFromCoord(currentCoord[0], currentCoord[1] - 1, numRows, numCols, nodeCartComm);
            rightRank = getRankFromCoord(currentCoord[0], currentCoord[1] + 1, numRows, numCols, nodeCartComm);

            int potentialNeighbors[4] = {upRank, downRank, leftRank, rightRank};
            //Check if the ranks retrieved are not duplicates and not the reporting node and not 
            for(int j = 0; j < MAX_NEIGHBORS; j++) {
                if(potentialNeighbors[j] != MPI_PROC_NULL && potentialNeighbors[j] < totalSize && potentialNeighbors[j] >= 0 && 
                checkingRank != potentialNeighbors[j] &&
                !isPresent(nearbyAvailable, index, potentialNeighbors[j]) && 
                !isPresent(neighbors, MAX_NEIGHBORS, potentialNeighbors[j])) {
    
                    nearbyAvailable[index++] = potentialNeighbors[j];
                }
            }
        }
    }
    *count = index;

}
