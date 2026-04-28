#include <stdio.h>
#include <unistd.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/mman.h>


typedef struct {
    int trolley, visitors, capacity;
    // time values in microseconds
    int trolley_travel_time;
    int max_queue_time; // for visitor untill they reach queue
    int min_cart_distance;

}   args_inputs;

typedef struct {
    // counters
    int action_counter; 
    int queue_counter; // counter how many people are in the line
    int capacity_counter; // counter the capacity of each trolley
    int visitors_counter; // counter for how many people will be standing in the line

    // semaphors
    sem_t queue_sem; // semaphor for people in queue
    sem_t visitors_sem; // semaphor for people in the trolley
    sem_t writing_sem; // semaphor for writing into the file 

} logical_system;

// assigns arguments to variables
int values_set_up(args_inputs *values, char **argv);

// control if the arguments meet the conditions
int check_values(args_inputs *values);

// memory allocation
logical_system *init_memory ();

logical_system *system_set_up();

void clean_memory();



int main(int argc, char **argv) {
    
    if(argc != 7) {

        fprintf(stderr, "Incorect number of arguments \n");
        return 1;
    }

    args_inputs values;
    
    if(values_set_up(&values, argv) == 1){
        return 1;
    }

    logical_system *shared_data = system_set_up();
    if(shared_data == NULL){
        return 1;
    }



    clean_memory(shared_data);
    
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

int check_values(args_inputs *values) {

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

    if(values->trolley_travel_time < 0 || values->trolley_travel_time > 1000) {
        fprintf(stderr, "Trolley travel time is out of the range \n");
        return 1;
    }

    if(values->max_queue_time < 0 || values->max_queue_time > 1000) {
        fprintf(stderr, "Queue time out of range \n");
        return 1;
    }

    if(values->min_cart_distance <= 0 || values->min_cart_distance > 1000) {
        fprintf(stderr, "Cart distance out of range \n");
        return 1;
    }   

    return 0;
}

logical_system *init_memory() {
    logical_system *shared_data = mmap(NULL, sizeof(logical_system), PROT_READ | PROT_WRITE, 
                                                                  MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    if(shared_data == MAP_FAILED) {
        fprintf(stderr, "mmap failed");
        return NULL;
    }
    return shared_data;
}

logical_system *system_set_up(){

    logical_system *shared_data = init_memory();
    if(shared_data == NULL){
        return;
    }

    sem_init(&shared_data->queue_sem, 1, 0);
    sem_init(&shared_data->visitors_sem, 1, 0);
    sem_init(&shared_data->writing_sem, 1, 1);

    shared_data->action_counter = 1;
    shared_data->queue_counter = 0;
    shared_data->capacity_counter = 0;
    shared_data->visitors_counter = 0;

    return shared_data
}

void clean_memory(logical_system *shared_data) {
    
    sem_destroy(&shared_data->queue_sem);
    sem_destroy(&shared_data->visitors_sem);
    sem_destroy(&shared_data->writing_sem);

    munmap(shared_data, sizeof(logical_system));
}
