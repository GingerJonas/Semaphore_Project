#include <stdio.h>
#include <unistd.h>
#include <semaphore.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <sys/wait.h>

// ---NEED TO ADD NEW SEMAPOHER FOR THE CART LEAVING--- \\ 

typedef struct {
    int cart, visitors, capacity;
    // time values in microseconds
    int cart_travel_time;
    int max_queue_time; // for visitor untill they reach queue
    int min_cart_distance;

}   args_inputs;

typedef struct {
    // counters
    int action_counter; 
    int queue_counter; // counter how many people are in the line
    int capacity_counter; // counter the capacity of each cart
    int visitors_counter; // counter for how many people will be standing in the line

    // semaphors
    sem_t queue_sem; // semaphor for people in queue
    sem_t visitors_sem; // semaphor for people in the cart
    sem_t writing_sem; // semaphor for writing into the file 
    sem_t dispatch_cart; // semaphor for trolley management
    sem_t cart_left; // semaphor when cart is leaving
    sem_t filled_cart; // semaphor for waiting for visitors to sit
    sem_t visitors_cart_left; // semaphor for visitors leaving the cart

} logical_system;

// assigns arguments to variables
int values_set_up(args_inputs *values, char **argv);

// control if the arguments meet the conditions
int check_values(args_inputs *values);

// memory allocation
logical_system *init_memory ();

// setup starting values for each variable and semaphore
logical_system *system_set_up();

void dispetcher_system(args_inputs *values, logical_system *shared_data, FILE *proj);

void cart_system(args_inputs *values, logical_system *shared_data, FILE *proj, int cart_id);

void park_system(args_inputs *values, logical_system *shared_data);

void clean_memory(logical_system *shared_data);

int main(int argc, char **argv) {
    
    // checks for wright amount of arguments
    if(argc != 7) {

        fprintf(stderr, "Incorect number of arguments \n");
        return 1;
    }

    args_inputs values;
    
    // set up values given from the arguments
    if(values_set_up(&values, argv) == 1){
        return 1;
    }

    // set up counters and semaphors before start
    logical_system *shared_data = system_set_up();
    if(shared_data == NULL){
        return 1;
    }

    FILE *proj;
    proj = fopen("proj2.out", "w"); // open file for writing output
    if(proj == NULL) {
        return 1;
    }

    // clean memory
    clean_memory(shared_data);
    
    return 0;
}

int values_set_up(args_inputs *values, char **argv) {

    values->cart = atoi(argv[1]);
    values->visitors = atoi(argv[2]);
    values->capacity = atoi(argv[3]);
    values->cart_travel_time = atoi(argv[4]);
    values->max_queue_time = atoi(argv[5]);
    values->min_cart_distance = atoi(argv[6]);

    if(check_values(values) == 1){
        return 1;
    }

    return 0;
}

int check_values(args_inputs *values) {

    if(values->cart <= 0 || values->cart >= 10) {
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

    if(values->cart_travel_time < 0 || values->cart_travel_time > 1000) {
        fprintf(stderr, "Trolley travel time is out of the range \n");
        return 1;
    }

    if(values->max_queue_time < 0 || values->max_queue_time > 1000) {
        fprintf(stderr, "Queue time out of range \n");
        return 1;
    }

    if(values->min_cart_distance <= 0 || values->min_cart_distance > 100) {
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
    // allocate memory
    logical_system *shared_data = init_memory();
    if(shared_data == NULL){
        return NULL;
    }
    // semaphor set up
    sem_init(&shared_data->queue_sem, 1, 0);
    sem_init(&shared_data->visitors_sem, 1, 0);
    sem_init(&shared_data->writing_sem, 1, 1);
    sem_init(&shared_data->dispatch_cart, 1, 0);
    sem_init(&shared_data->cart_left, 1, 0);
    sem_init(&shared_data->filled_cart, 1, 0);
    sem_init(&shared_data->visitors_cart_left, 1, 0);


    // variables set up
    shared_data->action_counter = 1;
    shared_data->queue_counter = 0;
    shared_data->capacity_counter = 0;
    shared_data->visitors_counter = 0;

    return shared_data;
}

void dispetcher_system(args_inputs *values, logical_system *shared_data, FILE *proj) {
    
    // semaphor sandwitch for friting into the file
    sem_wait(&shared_data->writing_sem);
    
    fprintf(proj, "%d: D: started\n", shared_data->action_counter); // print to the file
    shared_data->action_counter++; // raise actions
    
    sem_post(&shared_data->writing_sem); // end writing
    
    // doing loop untill no visitors are in the park
    while(shared_data->visitors_counter != values->visitors) {
        
        // doing another action for calling the cart
        sem_wait(&shared_data->writing_sem);
        fprintf(proj, "%d: D: next cart\n", shared_data->action_counter);
        shared_data->action_counter++;

        sem_post(&shared_data->writing_sem);

        // call the cart
        sem_post(&shared_data->dispatch_cart);

        // wrong semaphore need to be fixed
        sem_wait(&shared_data->dispatch_cart);

        sem_wait(&shared_data->cart_left);
        
        // stop untill the cart is in the safe distance 
        usleep(values->min_cart_distance * 1000); // in microseconds 
        

        shared_data->visitors_counter++; // raise visitors
    }

    sem_wait(&shared_data->writing_sem);

    fprintf(proj, "%d: D: closing", shared_data->action_counter);
    shared_data->action_counter++;

    sem_post(&shared_data->writing_sem);

    // checks if there is no cart left
    for(int i = 0; i <= values->cart; i++){
        sem_post(&shared_data->dispatch_cart);
    }

    exit(0);
}

void cart_system(args_inputs *values, logical_system *shared_data, FILE *proj, int cart_id)
{
    sem_wait(&shared_data->writing_sem);

    fprintf(proj, "%d: V %d: started\n", shared_data->action_counter, cart_id);
    shared_data->action_counter++;

    sem_post(&shared_data->writing_sem);

    while(true){
        // wait for dispatcher
        sem_wait(&shared_data->dispatch_cart);

        // break point
        if(shared_data->visitors_counter == values->visitors) {
            break;
        }
        
        int remaining_visitors = values->visitors - shared_data->visitors_counter; // visitors in park right now
        int cart_max_visitors = values->capacity; // fixed bug for the last ride of the day
        
        // for less people then there is capacity
        if(remaining_visitors < cart_max_visitors) {
            values->capacity = remaining_visitors;
        }

        // wait untill the cart is filled 
        for(int i = 0; i < values->capacity; i++){
            sem_post(&shared_data->queue_sem);
            sem_wait(&shared_data->filled_cart);
        }
        
        // anounce dispatcher that the cart left
        sem_post(&shared_data->cart_left);

        sem_wait(&shared_data->writing_sem);

        fprintf(proj, "%d: V %d: track\n", shared_data->action_counter, cart_id);
        shared_data->action_counter++;
        
        sem_post(&shared_data->writing_sem);
        
        // cart ride simulation
        usleep(values->cart_travel_time * 1000); // in microseconds

        sem_wait(&shared_data->writing_sem);

        fprintf(proj, "%d: V %d: unboard\n", shared_data->action_counter, cart_id);
        shared_data->action_counter++;
        
        sem_post(&shared_data->writing_sem);

        // unboard all visitors in the cart
        for(int i = 0; i < cart_max_visitors; i++) {
            sem_post(&shared_data->visitors_sem);
        }

        for(int i = 0; i < cart_max_visitors; i++){
            sem_wait(&shared_data->visitors_cart_left);
        }

    }

    exit(0);
}

void park_system(args_inputs *values, logical_system *shared_data){





}


void clean_memory(logical_system *shared_data) {
    
    sem_destroy(&shared_data->queue_sem);
    sem_destroy(&shared_data->visitors_sem);
    sem_destroy(&shared_data->writing_sem);
    sem_destroy(&shared_data->dispatch_cart);
    sem_destroy(&shared_data->cart_left);
    sem_destroy(&shared_data->filled_cart);
    sem_destroy(&shared_data->visitors_cart_left);

    munmap(shared_data, sizeof(logical_system));
}
