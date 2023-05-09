#ifndef NGSCOPE_TIME_STAMP_H
#define NGSCOPE_TIME_STAMP_H
#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdint.h>

/* Timestamp related function */
int64_t timestamp_ns();
int64_t timestamp_us();
int64_t timestamp_ms();

#endif
