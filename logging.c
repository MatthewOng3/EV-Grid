#include "logging.h"
#include "util.h"

 

/*-------------Create a log --------------*/
BaseAlert createAlertReport(int rank, int portsAvailable, int neighbors[MAX_NEIGHBORS], int neighborPortsAvail[MAX_NEIGHBORS], int numOfNeighbors, double commTimeBetweenNodes, int numOfMsg, int nearbyAvailable[MAX_NEARBY_EV], int nearbyCount){
    // Create the alert message
    BaseAlert alert;
    Time currentTime = getCurrentTime();  

    alert.year = currentTime.year;
    alert.month = currentTime.month;
    alert.day = currentTime.day;
    alert.hour = currentTime.hour;
    alert.minute = currentTime.minute;
    alert.second = currentTime.second;
    alert.rank = rank; 
    alert.portsAvailable = portsAvailable;
    alert.commTimeBetweenNodes = commTimeBetweenNodes;
    alert.numMsgsExchanged = numOfNeighbors;   
    alert.numOfNeighbors = numOfNeighbors;
    alert.nearbyNodesCount = nearbyCount;
    alert.startCommTime = MPI_Wtime();

    //Populate neighbors
    for (int i = 0; i < MAX_NEIGHBORS; i++) {
        alert.neighbors[i] = neighbors[i];
    }

    //Populate nodeStatus
    for (int i = 0; i < MAX_NEIGHBORS; i++) {
        alert.neighborPortsAvail[i] = neighborPortsAvail[i];
    }

    //Populate nearbyNodes
    for (int i = 0; i < MAX_NEARBY_EV; i++) {
        alert.nearbyNodes[i] = nearbyAvailable[i];
    }
    
    return alert;
}

/*Log the base alert to file output*/
void logBaseAlertToFile(BaseAlert alert, int iteration, CoordTuple* coordsArray, int nearbyAvailable[MAX_NEARBY_EV], int nearbyAvailableCount, int* freeNodes, int freeNodesCount, double baseCommTime, double commTime) {

    FILE *file = fopen("BaseAlert_log.txt", "a"); // Open the file in append mode

    if (!file) {
        perror("Failed to open the log file");
        return;
    }

    // Extracting the reported time from the BaseAlert struct into a new struct
    struct tm reported_tm;
    reported_tm.tm_year = alert.year - 1900;  // tm_year is years since 1900
    reported_tm.tm_mon = alert.month - 1;    // tm_mon is in the range [0, 11]
    reported_tm.tm_mday = alert.day;
    reported_tm.tm_hour = alert.hour;
    reported_tm.tm_min = alert.minute;
    reported_tm.tm_sec = alert.second;
    reported_tm.tm_isdst = -1;  

    // Automatically set the format
    mktime(&reported_tm);  
    char reported_buffer[80];
    //Format time into a string
    strftime(reported_buffer, 80, "%a %Y-%m-%d %H:%M:%S", &reported_tm); 
     
    time_t currTime;
    struct tm *timeInfo;

    time(&currTime); //Get current time and store it in currTime variable
    timeInfo = localtime(&currTime);//Convert to printable format

    char buffer[80];
    strftime(buffer, 80, "%a %Y-%m-%d %H:%M:%S", timeInfo);

    fprintf(file, "------------------------------------------------------------------------------------------------------------\n");
    fprintf(file, "Iteration : %d\n", iteration);
    fprintf(file, "Logged time : \t\t\t\t%s\n", buffer);
    fprintf(file, "Alert reported time : \t\t%s\n", reported_buffer); 
    fprintf(file, "Number of adjacent node: %d\n", alert.numOfNeighbors);
    fprintf(file, "Ports Available: %d\n", alert.portsAvailable);
    fprintf(file, "Availability to be considered full: %d\n\n",  alert.portsAvailable);

    fprintf(file, "Reporting Node \t\tCoord\t\tPort Value\tAvailable Port\n");
    fprintf(file, "%d\t\t\t\t\t(%d,%d)\t\t%d\t\t\t\t%d\n\n", alert.rank, coordsArray[alert.rank].x, coordsArray[alert.rank].y, PORTS, alert.portsAvailable);
    
    fprintf(file, "Adjacent Nodes\t\tCoord\t\tPort Value\tAvailable Port\n");

    for (int i = 0; i < MAX_NEIGHBORS; i++) {
        int nbrRank = alert.neighbors[i];
        if(nbrRank >= 0 && nbrRank != MPI_PROC_NULL){
            int x = coordsArray[nbrRank].x;
            int y = coordsArray[nbrRank].y;
            
            fprintf(file, "%d\t\t\t\t\t(%d,%d)\t\t%d\t\t\t\t%d\n", nbrRank, x, y, PORTS, alert.neighborPortsAvail[i]);
        }
    }

    fprintf(file, "\nNearby Nodes\t\tCoord\n");
    // This loop is just an example; you'll need to implement the actual logic for determining nearby nodes.
    for (int i = 0; i < nearbyAvailableCount; i++) { 
        fprintf(file, "%d\t\t\t\t\t(%d,%d)\n", nearbyAvailable[i], coordsArray[nearbyAvailable[i]].x, coordsArray[nearbyAvailable[i]].y); // Placeholder logic
    }

    fprintf(file, "\nAvailable station nearby (no report received in 10 seconds): ");
    for(int i = 0; i < freeNodesCount; i++){
        fprintf(file, "%d, ", freeNodes[i]);
    }
    printf("\n");
    fprintf(file, "\nCommunication Time between reporting node and its quadrant(seconds) : %f\n", alert.commTimeBetweenNodes);  
    fprintf(file, "Communication Time between reporting node and base station(seconds) : %f\n", baseCommTime);  
    fprintf(file, "Total Messages send between reporting node and it's neighbors: %d\n", alert.numMsgsExchanged); 
    fclose(file);
}
