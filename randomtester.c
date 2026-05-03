#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <limits.h>
#include <time.h>
#include <signal.h>

// ==========================================
// --- KONFIGURACE ---
// ==========================================
#define NUM_TESTS 100000
#define MAX_CORES 16
#define TIMEOUT_SEC 20
#define BINARY_PATH "./proj2"

atomic_size_t tests_done = 0;
atomic_size_t count_ok = 0;
atomic_size_t count_deadlock = 0;
atomic_size_t count_fail = 0;

char abs_binary_path[PATH_MAX];

void cleanup() {
    system("killall -q -9 proj2 >/dev/null 2>&1");
    system("ipcs -tm 2>/dev/null | grep \"$(whoami)\" | awk '{print $1}' | xargs -r ipcrm -m >/dev/null 2>&1");
    system("ipcs -ts 2>/dev/null | grep \"$(whoami)\" | awk '{print $1}' | xargs -r ipcrm -s >/dev/null 2>&1");
    system("find /dev/shm -user \"$(whoami)\" -delete >/dev/null 2>&1");
    usleep(100000);
}

void check_leftovers() {
    printf("\n📊 Kontrola zombiku a nezavrene pameti:\n");
    system("pgrep -x proj2 | wc -l | awk '{print \"Zbyva bezicich/zombie procesu proj2: \" $1}'");
    system("ipcs -m 2>/dev/null | grep \"$(whoami)\" | wc -l | awk '{print \"Zbyva alokovanych sdilenych pameti: \" $1}'");
    system("ipcs -s 2>/dev/null | grep \"$(whoami)\" | wc -l | awk '{print \"Zbyva alokovanych semaforu: \" $1}'");
}

void* worker_thread(void* arg) {
    long thread_id = (long)arg;
    char worker_dir[256];
    sprintf(worker_dir, "/dev/shm/proj2_worker_%ld", thread_id);
    mkdir(worker_dir, 0777);
    
    char sV[16], sN[16], sK[16], sTV[16], sTN[16], sO[16];
    int timeout_us = TIMEOUT_SEC * 1000000;
    
    unsigned int seed = time(NULL) ^ (getpid() << 16) ^ thread_id;

    while (1) {
        size_t current = atomic_fetch_add(&tests_done, 1);
        if (current >= NUM_TESTS) break;

        int V = (rand_r(&seed) % 9) + 1;         
        int N = (rand_r(&seed) % 200) + 1;       
        int K = (rand_r(&seed) % 37) + 4;        
        int TV = rand_r(&seed) % 1001;           
        int TN = rand_r(&seed) % 1001;           
        int O = (rand_r(&seed) % 100) + 1;       

        sprintf(sV, "%d", V);
        sprintf(sN, "%d", N);
        sprintf(sK, "%d", K);
        sprintf(sTV, "%d", TV);
        sprintf(sTN, "%d", TN);
        sprintf(sO, "%d", O);

        pid_t pid = fork();
        
        if (pid == 0) {
            chdir(worker_dir);
            setpgid(0, 0); 
            
            struct rlimit rl;
            rl.rlim_cur = 5 * 1024 * 1024; 
            rl.rlim_max = 5 * 1024 * 1024;
            setrlimit(RLIMIT_FSIZE, &rl);

            int null_fd = open("/dev/null", O_WRONLY);
            dup2(null_fd, STDOUT_FILENO);
            dup2(null_fd, STDERR_FILENO);
            close(null_fd);
            
            execl(abs_binary_path, "proj2", sV, sN, sK, sTV, sTN, sO, NULL);
            exit(255);
        } 
        else if (pid > 0) {
            int status;
            int elapsed_us = 0;
            int done = 0;
            
            while (elapsed_us < timeout_us) {
                pid_t res = waitpid(pid, &status, WNOHANG);
                if (res == pid) {
                    done = 1;
                    break;
                }
                usleep(250); 
                elapsed_us += 250;
            }
            
            if (!done) {
                kill(-pid, SIGKILL); 
                waitpid(pid, &status, 0); 
                atomic_fetch_add(&count_deadlock, 1);
                
                // Přerušíme "clean" řádek novým řádkem pro chybovou hlášku
                printf("\n❌ [DEADLOCK] Args: %s %s %s %s %s %s\n", sV, sN, sK, sTV, sTN, sO);
            } else {
                if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                    atomic_fetch_add(&count_ok, 1);
                } else {
                    atomic_fetch_add(&count_fail, 1);
                    // Přerušíme "clean" řádek novým řádkem pro chybovou hlášku
                    printf("\n❌ [FAIL_EXIT_CODE] Kod: %d | Args: %s %s %s %s %s %s\n", 
                           WEXITSTATUS(status), sV, sN, sK, sTV, sTN, sO);
                }
            }
        }
    }
    return NULL;
}

int main() {
    system("make clean >/dev/null 2>&1");
    system("make >/dev/null 2>&1");
    
    if (realpath(BINARY_PATH, abs_binary_path) == NULL) {
        printf("❌ Chyba: Binarka %s neexistuje.\n", BINARY_PATH);
        return 1;
    }

    printf("🧹 Provadim uklid pred testy...\n");
    cleanup();

    printf("🚀 Spoustim %d testu na %d jadrech...\n", NUM_TESTS, MAX_CORES);

    struct timespec start_time, current_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    pthread_t threads[MAX_CORES];
    for (long i = 0; i < MAX_CORES; i++) {
        pthread_create(&threads[i], NULL, worker_thread, (void*)i);
    }

    size_t last_reported = 0;
    while (1) {
        sleep(1);
        
        size_t current = atomic_load(&tests_done);
        if (current >= NUM_TESTS) break;
        
        if (current > last_reported) {
            clock_gettime(CLOCK_MONOTONIC, &current_time);
            double elapsed = (current_time.tv_sec - start_time.tv_sec) + 
                             (current_time.tv_nsec - start_time.tv_nsec) / 1e9;
            
            double tests_per_sec = current / elapsed;
            
            // --- MAGIE PRO CLEAN TERMINÁL ---
            // \r = vrať kurzor na začátek řádku
            // \033[K = vymaž zbytek řádku
            printf("\r[%zu/%d] Cas: %.1f s | OK:%zu DL:%zu FAIL:%zu | Rychlost: %.0f testu/s\033[K", 
                   current, NUM_TESTS, elapsed,
                   atomic_load(&count_ok), atomic_load(&count_deadlock), atomic_load(&count_fail),
                   tests_per_sec);
            fflush(stdout); // Nutnost, aby se to hned vykreslilo!
                   
            last_reported = current;
        }
    }

    // Odřádkování na konci smyčky, aby závěrečný report nepřepsal poslední stav
    printf("\n"); 

    for (int i = 0; i < MAX_CORES; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("\n========================================\n");
    printf("🎯 VYSLEDKY TESTOVANI\n");
    printf("========================================\n");
    printf("OK             : %zu\n", atomic_load(&count_ok));
    if (atomic_load(&count_deadlock) > 0) printf("DEADLOCK       : %zu\n", atomic_load(&count_deadlock));
    if (atomic_load(&count_fail) > 0)     printf("FAIL_EXIT_CODE : %zu\n", atomic_load(&count_fail));

    check_leftovers();
    printf("\n🧹 Finalni uklid...\n");
    cleanup();

    return 0;
}
