#include "util.h"

/*Get current time and return Time struct*/
Time getCurrentTime(){
    //
    time_t rawtime;
    struct tm * timeinfo;

    //Get current system times
    time(&rawtime);
    //Convert to local time
    timeinfo = localtime(&rawtime);

    Time newLog;
    
    //Set the relevant fields in the struct
    newLog.year = timeinfo->tm_year + 1900;
    newLog.month = timeinfo->tm_mon + 1;
    newLog.day = timeinfo->tm_mday;
    newLog.hour = timeinfo->tm_hour;
    newLog.minute = timeinfo->tm_min;
    newLog.second = timeinfo->tm_sec;
 
    return newLog;
}
/*Calculate difference between 2 time stamps*/
double calculateCommTime(BaseAlert receivedAlert) {
    // Get the current system time
    time_t raw_current_time;
    time(&raw_current_time);
    struct tm* utc_current_timeinfo = localtime(&raw_current_time);

 
    // Convert the UTC time struct to time_t for easy comparison later
    time_t utc_current_time = mktime(utc_current_timeinfo);

    // Convert the received time into time_t format
    struct tm received_timeinfo;
    received_timeinfo.tm_year = receivedAlert.year - 1900;
    received_timeinfo.tm_mon = receivedAlert.month - 1;
    received_timeinfo.tm_mday = receivedAlert.day;
    received_timeinfo.tm_hour = receivedAlert.hour;
    received_timeinfo.tm_min = receivedAlert.minute;
    received_timeinfo.tm_sec = receivedAlert.second;
    received_timeinfo.tm_isdst = 1; // Indicate that DST is  in effect for this timestamp

    // Convert the tm struct into time_t
    time_t received_time = mktime(&received_timeinfo);
 
    // Calculate the communication time in seconds
    double communication_time = difftime(utc_current_time, received_time);

 
    return communication_time;
}

/*Get the rank from coordinates*/
int getRankFromCoord(int x, int y, int numRows, int numCols, MPI_Comm nodeCartComm) {
    int inputCoords[2] = {x, y};
    int rank;

    if (inputCoords[0] >= 0 && inputCoords[0] < numRows && inputCoords[1] >= 0 && inputCoords[1] < numCols) {
        MPI_Cart_rank(nodeCartComm, inputCoords, &rank);
    } else {
        rank = -1;
    }
    
    return rank;
}
/*Check if a value is present in an array*/
int isPresent(int array[], int len, int value) {
    for (int i = 0; i < len; i++) {
        if (array[i] == value) {
            return 1;  // True, value is present in the array
        }
    }
    return 0;  // False, value is not present in the array
}

