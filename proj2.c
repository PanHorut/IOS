/**
 * @file proj2.c
 * @author Dominik Horut (xhorut01)
 * @date 22.04.2023
 * @brief IOS Project 2 - Semaphores (Barbershop problem style)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <linux/mman.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>

FILE *out;

/// Semaphores used for queues
sem_t *queue1sem, *queue2sem, *queue3sem;

/// Semaphores used to print output one at time
sem_t *output;
sem_t *mutex;

/// Semaphore used to give official signal to start serving service
sem_t *serve;

/// Semaphore used to make official go home only after the post office is closed 
sem_t *waitClose;

/// Shared variables representing number of customers in each queue
int *queue1, *queue2, *queue3;

/// Shared variable for printing number of line in proj2.out
int *line;

/// Shared variable which is set to 1, when post office closes, it is set to 0
int *isOpen;



/// @brief Function that inits all semaphores 
void init_semaphores(){

queue1sem = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
queue2sem = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
queue3sem = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);

output = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
mutex = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);

serve = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
waitClose = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);

sem_init(queue1sem, 1, 0);
sem_init(queue2sem, 1, 0);
sem_init(queue3sem, 1, 0);

sem_init(output, 1, 1);
sem_init(mutex, 1, 1);

sem_init(serve, 1, 0);
sem_init(waitClose, 1, 0);
}

/// @brief Function that inits all shared variables and sets their inital values
void init_variables(){
queue1 = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
queue2 = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
queue3 = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
line = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
isOpen = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);

*queue1 = 0;
*queue2 = 0;
*queue3 = 0;

/// We start counting from line 1
*line = 1;

/// Post office starts as opened
*isOpen = 1;
}

/// @brief Function that destroys all initialized semaphores, cleans shared memory and closes opened file
void cleanup(){

/// Destroying all semaphores
sem_destroy(queue1sem);
sem_destroy(queue2sem);
sem_destroy(queue3sem);
sem_destroy(output);
sem_destroy(mutex);
sem_destroy(serve);
sem_destroy(waitClose);

/// Cleaning all shared memory of semaphores
munmap(queue1sem, sizeof(sem_t));
munmap(queue2sem, sizeof(sem_t));
munmap(queue3sem, sizeof(sem_t));
munmap(output, sizeof(sem_t));
munmap(mutex, sizeof(sem_t));
munmap(serve, sizeof(sem_t));
munmap(waitClose, sizeof(sem_t));

/// Cleaning all shared memory of shared variables
munmap(queue1, sizeof(int));
munmap(queue2, sizeof(int));
munmap(queue3, sizeof(int));
munmap(line, sizeof(int));
munmap(isOpen, sizeof(int));

/// Closing proj2.out
fclose(out); 
}

/// @brief Function that checks if all arguments are valid
/// @param argc number of arguments
/// @param argv array of arguments
/// @return 0 if all argumetns are valid, else 1
int check_args(int argc, char *argv[]){
    /// Check that correct number of args is inserted
    if(argc != 6){
        fprintf(stderr, "Invalid number of arguments\n");
        return 1;
    }

    /// Check that NZ and NU are positive integers
    int NZ = atoi(argv[1]);
    int NU = atoi(argv[2]);
    if(NZ < 0 || NU < 0){
        fprintf(stderr, "NZ and NU must be positive integers\n");
        return 1;
    }
    
    /// Check that NU is not 0
    if(NU == 0){
        fprintf(stderr, "NU must me greater than zero\n");
        return 1;
    }

    /// Check that TZ is in correct interval
    int TZ = atoi(argv[3]);
    if(TZ < 0 || TZ > 10000){
        fprintf(stderr, "TZ must be in interval <0,10000>\n");
        return 1;
    }

    /// Check that TU is in correct interval
    int TU = atoi(argv[4]);
    if(TU < 0 || TU > 100){
        fprintf(stderr, "TU must be in interval <0,100>\n");
        return 1;
    }

    /// Check that F is in correct interval
    int F = atoi(argv[5]);
    if(F < 0 || F > 10000){
        fprintf(stderr, "F must be in interval <0,10000>\n");
        return 1;
    }
    
    /// Check that all arguments are integers and not strings or chars 
    for (int i = 1; i < argc; i++) {
        for (int j = 0; argv[i][j] != '\0'; j++) {
            
            if (!isdigit(argv[i][j])) {
                fprintf(stderr, "All arguments must be integers\n");
                return 1;
            }
        }
    }

    return 0;
}

/// @brief Function that creates specific seed everytime it is called
/// @return seed used for srand()
unsigned int get_random_seed(){

    struct timeval tv;
    gettimeofday(&tv, NULL);
    unsigned int seed = tv.tv_usec;
    return seed;
}

//My own printf to properly write into proj2.out (flush)

/// @brief Specific print to properly write into proj2.out (line numbers etc.) and to fflush after every line
/// @param stream file where we will write
/// @param format format of line
void my_print(FILE * stream, const char * format, ...){

  /// Semaphor used to print one line at time
  sem_wait(output);
  va_list args;
  va_start (args, format);

  /// Adding line number before every line
  fprintf(out, "%d: ", *line);

  vfprintf (stream, format, args);
  (*line)++;
  fflush(stream);
  va_end (args);
  sem_post(output);
}

/// @brief Function that creates subprocess for every customer
/// @param NZ number of customers to be "created"
/// @param TZ time (in miliseconds) for which customer will wait before he enters the post office
int generate_customers(int NZ, int TZ){

    for(int i = 1; i <= NZ; i++){
    pid_t ID = fork();
    if(ID == -1){
        fprintf(stderr, "Error when creating child process\n");
        return 1;
    }
    if(ID == 0){
        
        my_print(out, "Z %d: started\n", i);
        
        srand(get_random_seed());
        int waiting = (rand() % (TZ + 1));
        usleep(waiting*1000);
        if(*isOpen == 0){
            /// If customer came late 
            my_print(out, "Z %d: going home\n", i);
            exit(0);
        }

        /// If customer will make it in time
        srand(get_random_seed());
        int service = (rand() % (3)) + 1;
        my_print(out, "Z %d: entering office for a service %d\n",i, service);

        /// Customer picks one of the services       
        switch (service){
            
            /// Service 1
            case 1:
            sem_wait(mutex);
            /// Incrementing number of people in queue
            (*queue1)++;

            sem_post(mutex);
            sem_wait(queue1sem);

            my_print(out, "Z %d: called by office worker\n", i);

            /// Signal for official that he can serve this service 
            sem_post(serve);
            sem_wait(mutex);

            /// Decrementing number of people in queue
            (*queue1)--;
            sem_post(mutex);
            break;

            /// Service 2
            case 2:
            sem_wait(mutex);
            /// Incrementing number of people in queue
            (*queue2)++;

            sem_post(mutex);
            sem_wait(queue2sem);
            my_print(out, "Z %d: called by office worker\n", i);

            /// Signal for official that he can serve this service 
            sem_post(serve);
            sem_wait(mutex);

            /// Decrementing number of people in queue
            (*queue2)--;
            sem_post(mutex);
            break;

            /// Service 3
            case 3: 
            sem_wait(mutex);
            /// Incrementing number of people in queue
            (*queue3)++;

            sem_post(mutex);
            sem_wait(queue3sem);
            my_print(out, "Z %d: called by office worker\n", i);

            /// Signal for official that he can serve this service 
            sem_post(serve);
            sem_wait(mutex);

            /// Decrementing number of people in queue
            (*queue3)--;
            sem_post(mutex);
            break;

            default:
            break;
            } 

        /// After getting his service, customer waits for a while and then leaves, process ends
        srand(get_random_seed());
        int leaving = rand() % (11);
        usleep(leaving*1000);
        my_print(out, "Z %d: going home\n", i);
        exit(0);
        }
    }  
    return 0; 
}

/// @brief Function that creates subprocess for every official 
/// @param NU number of officials to be created
/// @param TU time (in miliseconds) that specifies maximum length of officials break
int generate_officials(int NU, int TU){
    

    for(int i = 1; i <= NU; i++){
        pid_t ID = fork();

        if(ID == -1){
        fprintf(stderr, "Error when creating child process\n");
        return 1;
        
        }
        if(ID == 0){
            my_print(out, "U %d: started\n", i);
            
            while(1){

                /// If there are no customers left to be served and post office is closed, official goes home
                if(*queue1 == 0 && *queue2 == 0 && *queue3 == 0 && *isOpen == 0){ 
                    break;

                /// If there are no customers to be served and post office is still opened, official takes a break    
                }else if(*queue1 == 0 && *queue2 == 0 && *queue3 == 0 && *isOpen == 1){

                    sem_wait(mutex);
                    my_print(out, "U %d: taking break\n", i);
                    sem_post(mutex);
                    srand(get_random_seed());
                    int nap = rand() % (TU + 1);
                    usleep(nap*1000);
                    sem_wait(mutex);
                    my_print(out, "U %d: break finished\n", i);
                    sem_post(mutex);
                /// Official picks non-empty queue and serves the service of that queue
                }else{
                srand(get_random_seed());
                int pickService = (rand() % 3) + 1;
                switch(pickService){
                    
                    /// Service 1
                    case 1: 

                    /// If queue is empty, proceeds to next queue
                    if(*queue1 == 0) continue;
                    sem_post(queue1sem);
                    

                    /// Waits for customer to give him permission to start serving a service
                    sem_wait(serve);
                    sem_wait(mutex);
                    my_print(out, "U %d: serving a service of type 1\n", i);
                    sem_post(mutex);

                    /// Serves a service
                    int processing1 = rand() % (11);
                    usleep((processing1)*1000);
                    my_print(out, "U %d: service finished\n", i);
                    break;

                    /// Service 2
                    case 2: 

                    /// If queue is empty, proceeds to next queue
                    if(*queue2 == 0) continue;
                    sem_post(queue2sem);
                    

                    /// Waits for customer to give him permission to start serving a service
                    sem_wait(serve);
                    sem_wait(mutex);
                    my_print(out, "U %d: serving a service of type 2\n", i);
                    sem_post(mutex);

                    /// Serves a service
                    int processing2 = rand() % (11);
                    usleep((processing2)*1000);
                    my_print(out, "U %d: service finished\n", i);
                    break;

                    /// Service 3
                    case 3: 

                    /// If queue is empty, proceeds to next queue
                    if(*queue3 == 0) continue;
                    sem_post(queue3sem);
                    

                    /// Waits for customer to give him permission to start serving a service
                    sem_wait(serve);
                    sem_wait(mutex);
                    my_print(out, "U %d: serving a service of type 3\n", i);
                    sem_post(mutex);

                    /// Serves a service
                    int processing3 = rand() % (11);
                    usleep((processing3)*1000);
                    my_print(out, "U %d: service finished\n", i);
                    break;

                    default:
                    break;
                    
                    }
                
                }
            }
            
            /// Waits for signal that the post office is closed and then goes home
            sem_wait(waitClose);
            sem_wait(mutex);
            my_print(out, "U %d: going home\n", i);
            sem_post(mutex);
            exit(0);
        }
    }
    return 0;
}

/// @brief Main function
/// @param argc number of arguments
/// @param argv array of arguments
/// @return 0 if everything is alright, 1 in case of error
int main(int argc, char *argv[]) {

/// Checking that all arguments are valid
if(check_args(argc, argv) == 1){
    return 1;
}

/// Initialiazing semaphores and shared variables
init_semaphores();
init_variables();

/// Opening proj2.out
out = fopen("proj2.out", "a");   

/// If proj2.out is not opened properly
if(out == NULL){
    fprintf(stderr, "File proj2.out was not opened succesfully\n");
    return 1;
}

/// Getting arguments
int NZ = atoi(argv[1]);
int NU = atoi(argv[2]);
int TZ = atoi(argv[3]);
int TU = atoi(argv[4]);
int F = atoi(argv[5]);

int errCheck;
/// Setting for how long post office will be opened
srand(get_random_seed());
int openTime = (rand() % (F - (F/2) + 1)) + (F/2);

/// Creating subprocesses of customers and officials
errCheck = generate_customers(NZ, TZ);

if(errCheck == 1){
    cleanup();
    exit(0);
} 
errCheck = generate_officials(NU, TU);

if(errCheck == 1){
    cleanup();
    exit(0);
} 

usleep(openTime*1000);

/// Setting post office as closed
(*isOpen) = 0;
my_print(out, "closing\n");

/// Letting all officials go home
for(int i = 0; i < NU; i++){
    sem_post(waitClose);
}


/// Cleaning shared memory
cleanup();
exit(0);

return 0;
}