#include <stdio.h>
#include <unistd.h>
#include <semaphore.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

typedef struct {
    int trolley, visitors, capacity;
    // time values in microseconds
    int trolley_travel_time;
    int max_queue_time; // for visitor untill they reach queue
    int min_cart_distance;

}   args_inputs;

int values_set_up(args_inputs *values, char **argv);

int check_values(args_inputs *values);


int main(int argc, char **argv) {
    
    if(argc != 7) {

        fprintf(stderr, "Incorect number of arguments \n");
        return 1;
    }

    args_inputs values;
    
    if(values_set_up(&values, argv) == 1){
        return 1;
    }

    return 0;
}

int values_set_up(args_inputs *values, char **argv) {

    values->trolley = atoi(argv[1]);
    values->visitors = atoi(argv[2]);
    values->capacity = atoi(argv[3]);
    values->trolley_travel_time = atoi(argv[4]);
    values->max_queue_time = atoi(argv[5]);
    values->min_cart_distance = atoi(argv[6]);

    if(check_values(values) == 1){
        return 1;
    }

    return 0;
}

int check_values(args_inputs *values){

    if(values->trolley <= 0 || values->trolley >= 10) {
        fprintf(stderr, "Number of trolley are out of range \n");
        return 1;
    }
    
    if(values->visitors <= 0 || values->visitors >= 10000) {
        fprintf(stderr, "Number of visitors are out of range \n");
        return 1;
    }

    if(values->capacity < 4 || values->capacity > 40) {
        fprintf(stderr, "Capacity out of range \n");
        return 1;
    }

    if(values->trolley_travel_time < 0 || values->trolley_travel_time >= 1000) {
        fprintf(stderr, "Trolley travel time is out of the range \n");
        return 1;
    }

    if(values->max_queue_time <= 0 || values->max_queue_time >= 1000) {
        fprintf(stderr, "Queue time out of range \n");
        return 1;
    }

    if(values->min_cart_distance <= 0 || values->min_cart_distance >= 1000) {
        fprintf(stderr, "Cart distance out of range \n");
        return 1;
    }   

    return 0;
}

