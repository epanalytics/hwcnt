#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include <papi.h>
#include "hwcnt.hpp"

namespace hwcnt {

#define PRINT_PREFIX "-[hwcnt]-\t"
#define MAX_STRING_LENGTH (1024)
#define MAX_ALLOWED_CPUS (384)

    volatile uint32_t ref_count = 0;

    void handle_papi_error(const char* msg, bool fatal){
        fprintf(stderr, PRINT_PREFIX "%s: %s\n", fatal?"error":"warn", msg);
        if (fatal){
            exit(-1);
        }
    }

#define PAPI_OR_DIE(__callname, __okret, __msg, ...)                    \
    if ((ret= PAPI_ ## __callname (__VA_ARGS__) ) != __okret){          \
        char __err_msg[MAX_STRING_LENGTH];                              \
        sprintf(__err_msg, "%s with code %d (%s)", __msg, ret, PAPI_strerror(ret)); \
        handle_papi_error(__err_msg, true);                             \
        assert(false && "should not arrive");                           \
    }

    papi::papi(std::vector<std::string>& evts, pid_t pid, bool inherit, bool multiplex){
        assert(ref_count == 0 && "Cannot create more than one hwcnt::papi object");
        ref_count++;

        assert(pid >= 0 && "Invalid pid passed in");
        process_only = pid;

        if (process_only){
            ncpus = 1;
        } else {
            ncpus = sysconf(_SC_NPROCESSORS_ONLN);
        }

        for (std::vector<std::string>::const_iterator it = evts.begin(); it != evts.end(); ++it){
            event_name_to_id[*it] = events.size();
            events.push_back(*it);
        }

        EventSets = new int[MAX_ALLOWED_CPUS];
        for (uint32_t cpu = 0; cpu < MAX_ALLOWED_CPUS; ++cpu){
            EventSets[cpu] = PAPI_NULL;
        }

        int ret;
        PAPI_OR_DIE(library_init, PAPI_VER_CURRENT, "PAPI_library_init failed", PAPI_VER_CURRENT);
        PAPI_OR_DIE(is_initialized, PAPI_LOW_LEVEL_INITED, "PAPI_is_initialized failed");
        PAPI_OR_DIE(thread_init, PAPI_OK, "PAPI_thread_init failed", pthread_self);
        if (multiplex){
            PAPI_OR_DIE(multiplex_init, PAPI_OK, "PAPI_multiplex_init failed");
        }

        int* event_codes = new int[events.size()];
        for (uint32_t evt = 0; evt < events.size(); ++evt){
            std::string name(events[evt]);
            char* cname = new char[name.size()+1];
            strcpy(cname, name.c_str());
            int code;
            char ermsg[MAX_STRING_LENGTH];
            sprintf(ermsg, "PAPI_event_name_to_code (%s) failed", cname);
            PAPI_OR_DIE(event_name_to_code, PAPI_OK, ermsg, cname, &code);
            delete[] cname;

            event_codes[evt] = code;
        }

        for (uint32_t cpu = 0; cpu < MAX_ALLOWED_CPUS; ++cpu){
            PAPI_OR_DIE(create_eventset, PAPI_OK, "PAPI_create_eventset failed", &EventSets[cpu]);

            if (process_only || multiplex){
                // TODO: probably don't always want to use 0 as the component id
                PAPI_OR_DIE(assign_eventset_component, PAPI_OK, "PAPI_assign_eventset_component failed", EventSets[cpu], 0);
            }

            if (multiplex){
                PAPI_OR_DIE(set_multiplex, PAPI_OK, "PAPI_set_multiplex failed", EventSets[cpu]);
            }

            for (uint32_t evt = 0; evt < events.size(); ++evt){
                char msg[MAX_STRING_LENGTH];
                sprintf(msg, "PAPI_add_event failed to add %s", events[evt].c_str());
                PAPI_OR_DIE(add_event, PAPI_OK, msg, EventSets[cpu], event_codes[evt]);
            }

            PAPI_option_t opt;
            memset(&opt, 0, sizeof(PAPI_option_t));

            if (process_only){
                if (inherit){
                    opt.inherit.inherit = PAPI_INHERIT_ALL;
                } else {
                    opt.inherit.inherit = PAPI_INHERIT_NONE;
                }
                opt.inherit.eventset = EventSets[cpu];

                PAPI_OR_DIE(set_opt, PAPI_OK, "PAPI_set_opt/PAPI_INHERIT failed", PAPI_INHERIT, &opt);
                if (cpu < ncpus)
                    PAPI_OR_DIE(attach, PAPI_OK, "PAPI_attach failed", EventSets[cpu], pid);
            } else {
                opt.cpu.eventset = EventSets[cpu];
                opt.cpu.cpu_num = cpu;

                if (cpu < ncpus)
                    PAPI_OR_DIE(set_opt, PAPI_OK, "PAPI_set_opt/PAPI_CPU_ATTACH failed", PAPI_CPU_ATTACH, &opt);
            }

            if (cpu < ncpus)
                PAPI_OR_DIE(start, PAPI_OK, "PAPI_start failed (you are trying to measure all CPUs and may not have permissions; check /proc/sys/kernel/perf_event_paranoid)", EventSets[cpu]);
        }

        delete[] event_codes;
    }

    papi::~papi(){
        assert(allocated.size() == 0);
        delete[] EventSets;
    }

    bool papi::is_alloced(papi_ctrs_t storage){
        return (allocated.count(uint64_t(storage)) == 1);
    }

    void papi::add_thread(pid_t tid){
        assert(ncpus < MAX_ALLOWED_CPUS && "Too many threads used... see the definition of MAX_ALLOWED_CPUS");

        int ret;
        PAPI_OR_DIE(attach, PAPI_OK, "PAPI_attached failed", EventSets[ncpus], tid);
        PAPI_OR_DIE(start, PAPI_OK, "PAPI_start failed (you are trying to measure all CPUs and may not have permissions; check /proc/sys/kernel/perf_event_paranoid)", EventSets[ncpus]);
        ncpus++;
    }

    papi_ctrs_t papi::salloc(){
        uint32_t numEvents = event_cnt();

        papi_ctrs_t storage = new uint64_t*[MAX_ALLOWED_CPUS];
        for (uint32_t cpu = 0; cpu < MAX_ALLOWED_CPUS; ++cpu){
            storage[cpu] = new uint64_t[numEvents];
            memset(storage[cpu], 0, sizeof(uint64_t)*numEvents);
        }
        allocated.insert(uint64_t(storage));
        return storage;
    }

    void papi::sfree(papi_ctrs_t storage){
        assert(is_alloced(storage));
        allocated.erase(uint64_t(storage));
        for (uint32_t cpu = 0; cpu < MAX_ALLOWED_CPUS; ++cpu){
            delete[] storage[cpu];
        }
        delete[] storage;
    }


    void papi::scopy(papi_ctrs_t dest, papi_ctrs_t src){
        assert(is_alloced(dest) && is_alloced(src));
        memcpy(dest, src, sizeof(uint64_t)*ncpus*event_cnt());
    }

    void papi::read(papi_ctrs_t storage, bool reset){
        assert(is_alloced(storage));
        uint32_t numEvents = event_cnt();
        long long int hwcValues[numEvents];
        int ret;

        // TODO: deal with overflowed counters
        for (uint32_t cpu = 0; cpu < ncpus; ++cpu){
            PAPI_OR_DIE(read, PAPI_OK, "PAPI_read failed", EventSets[cpu], hwcValues);
            if (reset){
                PAPI_OR_DIE(reset, PAPI_OK, "PAPI_reset failed", EventSets[cpu]);
            }
            for (uint32_t evt = 0; evt < numEvents; ++evt){
                set_ctr(storage, cpu, evt, hwcValues[evt]);
            }
        }
    }

    void papi::fini(papi_ctrs_t storage){
        assert(is_alloced(storage));
        uint32_t numEvents = event_cnt();
        long long int hwcValues[numEvents];
        int ret;

        for (uint32_t cpu = 0; cpu < ncpus; ++cpu){
            PAPI_OR_DIE(stop, PAPI_OK, "PAPI_stop failed", EventSets[cpu], hwcValues);
            for (uint32_t evt = 0; evt < numEvents; ++evt){
                set_ctr(storage, cpu, evt, hwcValues[evt]);
            }
        }
    }

    void papi::print(papi_ctrs_t storage){
        uint32_t numEvents = event_cnt();

        if (process_only){
            printf(PRINT_PREFIX "proc ");
        } else {
            printf(PRINT_PREFIX "cpu  ");
        }

        for (uint32_t evt = 0; evt < numEvents; ++evt){
            printf("\t%-16s", event_name(evt).c_str());
        }
        printf("\n");

        for (uint32_t cpu = 0; cpu < ncpus; ++cpu){
            if (process_only){
                printf(PRINT_PREFIX "%-5d", process_only);
            } else {
                printf(PRINT_PREFIX "%-5d", cpu);
            }

            for (uint32_t evt = 0; evt < numEvents; ++evt){
                printf("\t%-16ld", get_ctr(storage, cpu, evt));
            }
            printf("\n");
        }
    }

    void papi::set_ctr(papi_ctrs_t storage, uint32_t cpu, uint32_t eventid, uint64_t value){
        assert(is_alloced(storage));
        assert(cpu < ncpus);
        assert(eventid < event_cnt());
        storage[cpu][eventid] = value;
    }

    uint64_t papi::get_ctr(papi_ctrs_t storage, uint32_t cpu, uint32_t eventid){
        assert(is_alloced(storage));
        assert(cpu < ncpus);
        assert(eventid < event_cnt());
        return storage[cpu][eventid];
    }

    uint64_t papi::get_ctr(papi_ctrs_t storage, uint32_t cpu, std::string& eventname){
        assert(event_name_to_id.count(eventname) == 1);
        uint32_t id = event_name_to_id[eventname];
        return get_ctr(storage, cpu, id);
    }

    uint32_t papi::event_cnt(){
        return events.size();
    }

    uint32_t papi::cpu_cnt(){
        return ncpus;
    }

    std::string& papi::event_name(uint32_t idx){
        assert(idx < events.size());
        return events[idx];
    }

};

