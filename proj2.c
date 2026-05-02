#include <stdio.h>
#include <unistd.h>
#include <semaphore.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <sys/wait.h>

typedef struct {
    int cart, visitors, capacity;
    // time values in microseconds
    int cart_travel_time;
    int queue_arrival; // for visitor untill they reach queue
    int min_cart_distance;

}   args_inputs;

typedef struct {
    // counters
    int action_counter; 
    int queue_counter; // counter how many people are in the line
    int capacity_counter; // counter the capacity of each cart
    
    // semaphors
    sem_t queue_sem; // semaphor for people in queue
    sem_t visitors_sem; // semaphor for people in the cart
    sem_t writing_sem; // semaphor for writing into the file 
    sem_t dispatch_cart; // semaphor for trolley management
    sem_t cart_left; // semaphor when cart is leaving
    sem_t filled_cart; // semaphor for waiting for visitors to sit
    sem_t visitors_cart_left; // semaphor for visitors leaving the cart
    sem_t add_visitor; // semaphor to counter only one visitor at time 

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

void visitor_system(args_inputs *values, logical_system *shared_data, FILE *proj, int visitor_id);

void park_system(args_inputs *values, logical_system *shared_data, FILE *proj);

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

    // make the code faster
    setbuf(proj, NULL);

    // start the simulation
    park_system(&values, shared_data, proj);

    fclose(proj);
    clean_memory(shared_data);
    
    return 0;
}

int values_set_up(args_inputs *values, char **argv) {

    values->cart = atoi(argv[1]);
    values->visitors = atoi(argv[2]);
    values->capacity = atoi(argv[3]);
    values->cart_travel_time = atoi(argv[4]);
    values->queue_arrival = atoi(argv[5]);
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

    if(values->queue_arrival < 0 || values->queue_arrival > 1000) {
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
    sem_init(&shared_data->dispatch_cart, 1, 0);
    sem_init(&shared_data->cart_left, 1, 0);
    sem_init(&shared_data->filled_cart, 1, 0);
    sem_init(&shared_data->visitors_cart_left, 1, 0);

    // mutex semaphor set up
    sem_init(&shared_data->writing_sem, 1, 1);
    sem_init(&shared_data->add_visitor, 1, 1);

    // variables set up
    shared_data->action_counter = 1;
    shared_data->queue_counter = 0;
    shared_data->capacity_counter = 0;

    return shared_data;
}

void dispetcher_system(args_inputs *values, logical_system *shared_data, FILE *proj) {
    
    // semaphor sandwitch for friting into the file
    sem_wait(&shared_data->writing_sem);
    
    fprintf(proj, "%d: D: started\n", shared_data->action_counter); // print to the file
    shared_data->action_counter++; // raise actions
    
    sem_post(&shared_data->writing_sem); // end writing
    
    // doing loop untill no visitors are in the park
    while(shared_data->queue_counter != values->visitors) {
        
        // doing another action for calling the cart
        sem_wait(&shared_data->writing_sem);
        fprintf(proj, "%d: D: next cart\n", shared_data->action_counter);
        shared_data->action_counter++;

        sem_post(&shared_data->writing_sem);

        // call the cart
        sem_post(&shared_data->dispatch_cart);

        // wait untill cart leave
        sem_wait(&shared_data->cart_left);
        
        // stop untill the cart is in the safe distance 
        usleep(values->min_cart_distance * 1000); // in microseconds 
        
    }

    sem_wait(&shared_data->writing_sem);

    fprintf(proj, "%d: D: closing\n", shared_data->action_counter);
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

        sem_wait(&shared_data->add_visitor);

        int remaining_visitors = values->visitors - shared_data->queue_counter; // visitors in park right now
        int cart_max_visitors = values->capacity; // changing value localy 

        
        // for less people then there is capacity
        if(remaining_visitors < cart_max_visitors) {
            cart_max_visitors = remaining_visitors;
        }

        // claim the visitors
        shared_data->queue_counter += cart_max_visitors;

        sem_post(&shared_data->add_visitor);

        if(cart_max_visitors == 0) {
            break;
        }

        sem_wait(&shared_data->writing_sem);

            fprintf(proj, "%d: V %d: boarding started\n", shared_data->action_counter, cart_id);
            shared_data->action_counter++;
        
        sem_post(&shared_data->writing_sem);


        // wait untill the cart is filled 
        for(int i = 0; i < cart_max_visitors; i++) {
            sem_post(&shared_data->queue_sem);
            sem_wait(&shared_data->filled_cart);
        }
        
        // anounce dispatcher that the cart left
        sem_post(&shared_data->cart_left);

        sem_wait(&shared_data->writing_sem);

        fprintf(proj, "%d: V %d: boarding complete\n", shared_data->action_counter, cart_id);
        shared_data->action_counter++;
        
        sem_post(&shared_data->writing_sem);
        
        // cart ride simulation
        usleep(values->cart_travel_time * 1000); // in microseconds

        sem_wait(&shared_data->writing_sem);

        fprintf(proj, "%d: V %d: leaving started\n", shared_data->action_counter, cart_id);
        shared_data->action_counter++;
        
        sem_post(&shared_data->writing_sem);

        // unboard all visitors in the cart
        for(int i = 0; i < cart_max_visitors; i++) {
            sem_post(&shared_data->visitors_sem);
        }

        for(int i = 0; i < cart_max_visitors; i++) {
            sem_wait(&shared_data->visitors_cart_left);
        }

        sem_wait(&shared_data->writing_sem);

        fprintf(proj, "%d: V %d: leaving complete\n", shared_data->action_counter, cart_id);
        shared_data->action_counter++;
        
        sem_post(&shared_data->writing_sem);

    }

    sem_wait(&shared_data->writing_sem);

        fprintf(proj, "%d: V %d: closed\n", shared_data->action_counter, cart_id);
        shared_data->action_counter++;
        
    sem_post(&shared_data->writing_sem);


    exit(0);
}

void visitor_system(args_inputs *values, logical_system *shared_data, FILE *proj, int visitor_id) {

    sem_wait(&shared_data->writing_sem);

        fprintf(proj, "%d: N %d: started\n", shared_data->action_counter, visitor_id);
        shared_data->action_counter++;
        
    sem_post(&shared_data->writing_sem);

    // wait untill visitor come to the queue
    usleep(rand() % (values->queue_arrival + 1));

    sem_wait(&shared_data->writing_sem);

        fprintf(proj, "%d: N %d: queue\n", shared_data->action_counter, visitor_id);
        shared_data->action_counter++;
        
    sem_post(&shared_data->writing_sem);

    // wait for the cart arrival
    sem_wait(&shared_data->queue_sem);

    sem_wait(&shared_data->writing_sem);

        fprintf(proj, "%d: N %d: boarding\n", shared_data->action_counter, visitor_id);
        shared_data->action_counter++;
        
    sem_post(&shared_data->writing_sem);

    // visitor is seated
    sem_post(&shared_data->filled_cart);

    // wait untill the ride is over
    sem_wait(&shared_data->visitors_sem);

    sem_wait(&shared_data->writing_sem);

        fprintf(proj, "%d: N %d: leaving\n", shared_data->action_counter, visitor_id);
        shared_data->action_counter++;
        
    sem_post(&shared_data->writing_sem);

    // visitor is leaving
    sem_post(&shared_data->visitors_cart_left);

    sem_wait(&shared_data->add_visitor);
    
    sem_post(&shared_data->add_visitor);

    exit(0);
}


void park_system(args_inputs *values, logical_system *shared_data, FILE *proj) {

    int cart_id;
    int visitor_id;

    pid_t pid = fork();
    if(pid == 0) {

        dispetcher_system(values, shared_data, proj);

        exit(0);
    } else if(pid < 0) {
        fprintf(stderr, "Fork failed \n");
        exit(1);
    }

    // generate carts 
    for(int i = 0; i < values->cart; i++) {

        pid = fork();
        if(pid == 0) {

            cart_id = i + 1;
            cart_system(values, shared_data, proj, cart_id);
        
            exit(0);
        } else if(pid < 0) {
            fprintf(stderr, "Fork failed \n");
            exit(1);
        }

    }

    // generate visitors
    for(int i = 0; i < values->visitors; i++) {

        pid = fork();
        if(pid == 0) {

            visitor_id = i + 1;
            visitor_system(values, shared_data, proj, visitor_id);

            exit(0);
        } else if(pid < 0){
            fprintf(stderr, "Fork failed \n");
            exit(1);
        }

    }

    int all_entities = 1 + values->cart + values->visitors;

    // original entity wait for the copies to finish
    for(int i = 0; i < all_entities; i++){
        wait(NULL);
    }


}


void clean_memory(logical_system *shared_data) {
    
    sem_destroy(&shared_data->queue_sem);
    sem_destroy(&shared_data->visitors_sem);
    sem_destroy(&shared_data->writing_sem);
    sem_destroy(&shared_data->dispatch_cart);
    sem_destroy(&shared_data->cart_left);
    sem_destroy(&shared_data->filled_cart);
    sem_destroy(&shared_data->visitors_cart_left);
    sem_destroy(&shared_data->add_visitor);

    munmap(shared_data, sizeof(logical_system));
}
