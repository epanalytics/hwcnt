// simple example of using hwcnt library to launch a process and collect hwc stats for it
#include <sys/wait.h>
#include <assert.h>
#include <unistd.h>
#include <stdio.h>

#include "hwcnt.h"

#define NUM_EVENTS (6)

int main(int argc, char** argv){
    int status;
    uint32_t evt, evt_count;
    hwcnt_papi_t ctr;
    hwcnt_papi_ctrs_t storage;
    pid_t pid;
    char* events[NUM_EVENTS];

    assert(argc > 1 && "supply a program/arguments as args");

    events[0] = "PAPI_TOT_INS";
    events[1] = "PAPI_TOT_CYC";
    events[2] = "PAPI_LD_INS";
    events[3] = "PAPI_SR_INS";
    events[4] = "PAPI_BR_INS";
    events[5] = "PAPI_L1_ICM";

    pid = fork();
    if (pid == 0){
        execvp(argv[1], &(argv[1]));
        assert(0 && "should not arrive");
    }

    // passing zero as pid counts events on all cpus
    // passing 1 for multiplexing allows us to support more than the physically available number of counters
    ctr = hwcnt_papi_create(events, NUM_EVENTS, 0, 0, 1);
    storage = hwcnt_papi_salloc(ctr);

    while (waitpid(pid, &status, WNOHANG) == 0){
        // read counters, resetting them
        hwcnt_papi_read(ctr, storage, 1);
        hwcnt_papi_print(ctr, storage);
        sleep(1);
    }

    hwcnt_papi_fini(ctr, storage);
    hwcnt_papi_print(ctr, storage);

    evt_count = hwcnt_papi_event_cnt(ctr);
    assert(evt_count == NUM_EVENTS);
    for (evt = 0; evt < evt_count; ++evt){
        // cpu==0 when in process mode
        printf("Final counter value: %s=%ld\n", events[evt], hwcnt_papi_get_ctr(ctr, storage, 0, evt));
    }

    hwcnt_papi_sfree(ctr, storage);
    hwcnt_papi_destroy(ctr);

    return 0;
}
