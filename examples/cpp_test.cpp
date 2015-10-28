// simple example of using hwcnt library to launch a process and collect hwc stats for it
#include <sys/wait.h>
#include <assert.h>
#include <unistd.h>
#include <stdio.h>

#include "hwcnt.hpp"

int main(int argc, char** argv){
    int status;
    assert(argc > 1 && "supply a program/arguments as args");

    // its ok to mix native/preset events
    std::vector<std::string> events;
    events.push_back("perf::PERF_COUNT_HW_CACHE_REFERENCES");
    events.push_back("PAPI_TOT_CYC");

    // counts events for this process and all children created in the future
    hwcnt::papi ctr = hwcnt::papi(events, getpid(), true, false);

    // allocate counter storage
    hwcnt::papi_ctrs_t storage = ctr.salloc();

    pid_t pid = fork();
    if (pid == 0){
        execvp(argv[1], &(argv[1]));
        assert(0 && "should not arrive");
    }

    while (waitpid(pid, &status, WNOHANG) == 0){
        // read counters, don't reset them so they continue to accumulate
        ctr.read(storage, false);
        ctr.print(storage);
        sleep(1);
    }

    ctr.fini(storage);
    for (uint32_t evt = 0; evt < events.size(); ++evt){
        printf("Final counter value: %s=%ld\n", ctr.event_name(evt).c_str(), ctr.get_ctr(storage, 0, evt));
    }

    ctr.sfree(storage);

    return 0;
}
