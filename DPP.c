#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

#define LEFT_FORK_ID(philID) (philID)
#define RIGHT_FORK_ID(philID) ((numPhil + philID - 1) % numPhil)
#define SMALLER_FORK_ID(philID) (LEFT_FORK_ID(philID)<RIGHT_FORK_ID(philID)\
                                  ? LEFT_FORK_ID(philID) : RIGHT_FORK_ID(philID))
#define LARGER_FORK_ID(philID) (LEFT_FORK_ID(philID)<RIGHT_FORK_ID(philID)\
                                  ? RIGHT_FORK_ID(philID) : LEFT_FORK_ID(philID))
#define FREE -1

typedef enum {
  EATING,
  THINKING,
  WAITING,
  TERMINATED,
} philState_t;
void *philosopher(void *arg);
void think(unsigned philID);
void eat(unsigned philID);
void acquireForks(unsigned philID);
void releaseForks(unsigned philID);
void *watcher(void *arg);
void init();
void start();
void terminate();
pthread_t watcherHandler, *phils;
philState_t *philStates;
int *forkStates;
pthread_mutex_t contLock, stateLock, *forks;
sem_t initReady, procPermit, watcherPermit;
unsigned numPhil, seed, cont = 0;

int main(int argc, char const *argv[]) {
  if (argc != 4) {
    fprintf(stderr, "Usage: DPP [number of philosophers] [random seed] [time limit].\n");
    return EXIT_FAILURE;
  }
  numPhil = atoi(argv[1]);
  seed = atoi(argv[2]);
  int timeLim = atoi(argv[3]);
  init();
  start();
  sleep(timeLim);
  terminate();
  return EXIT_SUCCESS;
}

void *philosopher(void *arg) {
  unsigned id = *((unsigned*)arg);
  free(arg);
  sem_post(&initReady);
  sem_wait(&procPermit);
  while (1) {
    think(id);
    eat(id);
  }
  pthread_exit(NULL);
}

void *watcher(void *arg) {
  sem_post(&initReady);
  sem_wait(&watcherPermit);
  while (1) {
    pthread_mutex_lock(&stateLock);
    size_t numThinking = 0, numEating = 0, numWaiting = 0, numTerm = 0;
    size_t numUse = 0, numAvail = 0;
    printf("Philo   State             Fork    Held by\n");
    size_t i;
    for (i = 0; i < numPhil; ++i) {
      printf("[%2lu]:   ", i);
      switch (philStates[i]) {
        case THINKING:
          printf("%-18s", "Thinking");
          numThinking++;
          break;
        case EATING:
          printf("%-18s", "Eating");
          numEating++;
          break;
        case WAITING:
          printf("%-18s", "Waiting");
          numWaiting++;
          break;
        case TERMINATED:
          printf("%-18s", "Terminated");
          numTerm++;
      }
      printf("[%2lu]:   ", i);
      if (forkStates[i] == FREE) {
        printf("Free\n");
        numAvail++;
      } else {
        printf("%4d\n", forkStates[i]);
        numUse++;
      }
    }
    printf("Th=%2lu Wa=%2lu Ea=%2lu         ", numThinking, numWaiting, numEating);
    printf("Use=%2lu  Avail=%2lu\n\n\n", numUse, numAvail);
    if (numTerm == numPhil) {
      pthread_mutex_unlock(&stateLock);
      pthread_exit(NULL);
    }
    pthread_mutex_unlock(&stateLock);
    usleep(500000);
  }
}

void think(unsigned id) {
  pthread_mutex_lock(&stateLock);
  philStates[id] = THINKING;
  pthread_mutex_unlock(&stateLock);
  usleep(random() % 10000000 + 1);
  pthread_mutex_lock(&contLock);
  if (!cont) {
    pthread_mutex_lock(&stateLock);
    philStates[id] = TERMINATED;
    pthread_mutex_unlock(&stateLock);
    pthread_mutex_unlock(&contLock);
    pthread_exit(NULL);
  }
  pthread_mutex_unlock(&contLock);
}

void eat(unsigned id) {
  pthread_mutex_lock(&stateLock);
  philStates[id] = WAITING;
  pthread_mutex_unlock(&stateLock);
  acquireForks(id);
  usleep(random() % 10000000 + 1);
  releaseForks(id);
}

void acquireForks(unsigned philID) {
  pthread_mutex_lock(&forks[SMALLER_FORK_ID(philID)]);
  pthread_mutex_lock(&stateLock);
  forkStates[SMALLER_FORK_ID(philID)] = philID;
  pthread_mutex_unlock(&stateLock);
  pthread_mutex_lock(&forks[LARGER_FORK_ID(philID)]);
  pthread_mutex_lock(&stateLock);
  forkStates[LARGER_FORK_ID(philID)] = philID;
  philStates[philID] = EATING;
  pthread_mutex_unlock(&stateLock);
}

void releaseForks(unsigned philID) {
  pthread_mutex_lock(&stateLock);
  pthread_mutex_unlock(&forks[LEFT_FORK_ID(philID)]);
  forkStates[LEFT_FORK_ID(philID)] = FREE;
  pthread_mutex_unlock(&forks[RIGHT_FORK_ID(philID)]);
  forkStates[RIGHT_FORK_ID(philID)] = FREE;
  pthread_mutex_lock(&contLock);
  if (!cont) {
    philStates[philID] = TERMINATED;
    pthread_mutex_unlock(&stateLock);
    pthread_mutex_unlock(&contLock);
    pthread_exit(NULL);
  }
  pthread_mutex_unlock(&contLock);
  philStates[philID] = THINKING;
  pthread_mutex_unlock(&stateLock);
}

void init() {
  pthread_mutex_init(&contLock, NULL);
  pthread_mutex_init(&stateLock, NULL);
  sem_init(&initReady, 0, 0);
  sem_init(&procPermit, 0, 0);
  sem_init(&watcherPermit, 0, 0);
  phils = (pthread_t*)malloc(numPhil * sizeof(pthread_t));
  forks = (pthread_mutex_t*)malloc(numPhil * sizeof(pthread_mutex_t));
  philStates = (philState_t*)malloc(numPhil * sizeof(philState_t));
  forkStates = (int*)malloc(numPhil * sizeof(int));
  srandom(seed);
  size_t i;
  for (i = 0; i < numPhil; ++i) {
    pthread_mutex_init(&forks[i], NULL);
    forkStates[i] = FREE;
    unsigned *arg = (unsigned*)malloc(sizeof(unsigned));
    *arg = i;
    pthread_create(&phils[i], NULL, philosopher, (void*)arg);
    sem_wait(&initReady);
  }
  pthread_create(&watcherHandler, NULL, watcher, NULL);
  sem_wait(&initReady);
}

void start() {
  cont = 1;
  size_t i;
  for (i = 0; i < numPhil; ++i) {
    sem_post(&procPermit);
  }
  sem_post(&watcherPermit);
}

void terminate() {
  pthread_mutex_lock(&contLock);
  cont = 0;
  pthread_mutex_unlock(&contLock);
  size_t i;
  for (i = 0; i < numPhil; ++i) {
    pthread_join(phils[i], NULL);
  }
  pthread_join(watcherHandler, NULL);
  pthread_mutex_destroy(&contLock);
  pthread_mutex_destroy(&stateLock);
  for (i = 0; i < numPhil; ++i) {
    pthread_mutex_destroy(&forks[i]);
  }
  sem_destroy(&initReady);
  sem_destroy(&procPermit);
  sem_destroy(&watcherPermit);
  free(phils);
  free(forks);
  free(philStates);
  free(forkStates);
}
