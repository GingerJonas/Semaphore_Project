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
#define MAX_CORES 16
#define TIMEOUT_SEC 20
#define BINARY_PATH "./proj2"

// --- POČTY TESTOVANÝCH HODNOT ---
// Zde si jednoduše měníš, kolik hodnot se má pro každý parametr zkusit.
#define COUNT_V 9    // Kapacita vozíku (max 9, takže zkusí všechny)
#define COUNT_N 20   // Počet zákazníků
#define COUNT_K 15   // Kapacita stanice
#define COUNT_TV 3   // MAX čas jízdy (stačí 3: např. 0, 1, 1000)
#define COUNT_TN 3   // MAX čas nástupu (stačí 3: např. 0, 1, 1000)
#define COUNT_O 10   // Opakování

// Atomické počítadla pro výsledky
atomic_size_t tests_done = 0;
atomic_size_t count_ok = 0;
atomic_size_t count_deadlock = 0;
atomic_size_t count_fail = 0;

char abs_binary_path[PATH_MAX];
size_t TOTAL_TESTS = 0;

// Pole pro vygenerované hodnoty dynamicky podle tvého nastavení
int V_vals[COUNT_V], N_vals[COUNT_N], K_vals[COUNT_K];
int TV_vals[COUNT_TV], TN_vals[COUNT_TN], O_vals[COUNT_O];
int V_len, N_len, K_len, TV_len, TN_len, O_len;

// Pomocná funkce pro řazení
int cmpfunc(const void *a, const void *b) {
    return (*(int*)a - *(int*)b);
}

// Bezpečná verze generátoru (zvládne i count = 1, 2 nebo 3 bez přetečení paměti)
int get_smart_values(int min_v, int max_v, int count, int *out) {
    if (count <= 0) return 0;
    
    if (max_v - min_v + 1 <= count) {
        for (int i = 0; i <= max_v - min_v; i++) out[i] = min_v + i;
        return max_v - min_v + 1;
    }
    
    int temp[1000]; // Dočasný buffer pro výběr
    int idx = 0;
    
    // Nejvyšší priorita: extrémy
    if (count >= 1) temp[idx++] = min_v;
    if (count >= 2) temp[idx++] = max_v;
    if (count >= 3) temp[idx++] = min_v + 1;
    if (count >= 4) temp[idx++] = max_v - 1;
    
    // Zbytek vyplníme rovnoměrně
    int rem = count - 4;
    if (rem > 0) {
        double step = (double)(max_v - min_v) / (rem + 1);
        for (int i = 1; i <= rem; i++) {
            temp[idx++] = min_v + (int)(step * i);
        }
    }
    
    // Seřazení a odstranění duplicit
    qsort(temp, idx, sizeof(int), cmpfunc);
    int unique = 0;
    for(int i = 0; i < idx; i++) {
        if (i == 0 || temp[i] != temp[i-1]) {
            if (unique < count) {
                out[unique++] = temp[i];
            }
        }
    }
    return unique;
}

void cleanup() {
    system("killall -q -9 proj2 >/dev/null 2>&1");
    system("ipcs -tm 2>/dev/null | grep \"$(whoami)\" | awk '{print $1}' | xargs -r ipcrm -m >/dev/null 2>&1");
    system("ipcs -ts 2>/dev/null | grep \"$(whoami)\" | awk '{print $1}' | xargs -r ipcrm -s >/dev/null 2>&1");
    system("rm -rf /dev/shm/proj2_worker_* >/dev/null 2>&1");
    system("find /dev/shm -user \"$(whoami)\" -delete >/dev/null 2>&1");
    usleep(100000);
}

void check_leftovers() {
    printf("\n📊 Kontrola zombiku a nezavrene pameti:\n");
    system("pgrep -x proj2 | wc -l | awk '{print \"Zbyva procesu proj2: \" $1}'");
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

    while (1) {
        size_t current = atomic_fetch_add(&tests_done, 1);
        if (current >= TOTAL_TESTS) break;

        size_t temp = current;
        int o_val  = O_vals[temp % O_len]; temp /= O_len;
        int tn_val = TN_vals[temp % TN_len]; temp /= TN_len;
        int tv_val = TV_vals[temp % TV_len]; temp /= TV_len;
        int k_val  = K_vals[temp % K_len]; temp /= K_len;
        int n_val  = N_vals[temp % N_len]; temp /= N_len;
        int v_val  = V_vals[temp % V_len];

        sprintf(sV, "%d", v_val);
        sprintf(sN, "%d", n_val);
        sprintf(sK, "%d", k_val);
        sprintf(sTV, "%d", tv_val);
        sprintf(sTN, "%d", tn_val);
        sprintf(sO, "%d", o_val);

        pid_t pid = fork();
        
        if (pid == 0) {
            chdir(worker_dir);
            setpgid(0, 0); // Ochrana proti zombíkům
            
            struct rlimit rl;
            rl.rlim_cur = 5 * 1024 * 1024; // Limit 5 MB
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
                kill(-pid, SIGKILL); // Zabije celý strom procesů
                waitpid(pid, &status, 0); 
                atomic_fetch_add(&count_deadlock, 1);
                printf("\n❌ [DEADLOCK] Args: %s %s %s %s %s %s\n", sV, sN, sK, sTV, sTN, sO);
            } else {
                if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                    atomic_fetch_add(&count_ok, 1);
                } else {
                    atomic_fetch_add(&count_fail, 1);
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

    // Nyní předáváme makra pro velikosti
    V_len  = get_smart_values(1, 9, COUNT_V, V_vals);
    N_len  = get_smart_values(1, 200, COUNT_N, N_vals);
    K_len  = get_smart_values(4, 40, COUNT_K, K_vals);
    TV_len = get_smart_values(0, 1000, COUNT_TV, TV_vals);
    TN_len = get_smart_values(0, 1000, COUNT_TN, TN_vals);
    O_len  = get_smart_values(1, 100, COUNT_O, O_vals);

    TOTAL_TESTS = (size_t)V_len * N_len * K_len * TV_len * TN_len * O_len;

    printf("🧹 Provadim uklid pred testy...\n");
    cleanup();

    printf("🚀 Spoustim %zu testu na %d jadrech...\n", TOTAL_TESTS, MAX_CORES);

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
        if (current >= TOTAL_TESTS) break;
        
        if (current > last_reported) {
            clock_gettime(CLOCK_MONOTONIC, &current_time);
            double elapsed = (current_time.tv_sec - start_time.tv_sec) + 
                             (current_time.tv_nsec - start_time.tv_nsec) / 1e9;
            
            double tests_per_sec = current / elapsed;
            double remaining_sec = (TOTAL_TESTS - current) / (tests_per_sec > 0 ? tests_per_sec : 1);
            
            printf("[%zu/%zu] Stav: OK:%zu DL:%zu FAIL:%zu | Rychlost: %.0f testu/s | Zbyva: %.1f sec\n", 
                   current, TOTAL_TESTS, 
                   atomic_load(&count_ok), atomic_load(&count_deadlock), atomic_load(&count_fail),
                   tests_per_sec, remaining_sec);
                   
            last_reported = current;
        }
    }

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
