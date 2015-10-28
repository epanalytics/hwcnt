#include "hwcnt.h"
#undef __hwcnt_is_defined__
#include "hwcnt.hpp"

extern "C" {
    hwcnt_papi_t hwcnt_papi_create(char** events, uint32_t evt_count, pid_t pid, int inherit, int multiplex){
        std::vector<std::string> events_vect;
        for (uint32_t evt = 0; evt < evt_count; ++evt){
            events_vect.push_back(std::string(events[evt]));
        }
        hwcnt::papi* ctr = new hwcnt::papi(events_vect, pid, inherit==0?false:true, multiplex==0?false:true);
        return (hwcnt_papi_t)ctr;
    }

    void hwcnt_papi_destroy(hwcnt_papi_t papi){
        hwcnt::papi* cpapi = (hwcnt::papi*)papi;
        delete cpapi;
    }

    hwcnt_papi_ctrs_t hwcnt_papi_salloc(hwcnt_papi_t papi){
        hwcnt::papi* cpapi = (hwcnt::papi*)papi;
        hwcnt::papi_ctrs_t storage = cpapi->salloc();
        return (hwcnt_papi_ctrs_t)storage;
    }

    void hwcnt_papi_sfree(hwcnt_papi_t papi, hwcnt_papi_ctrs_t storage){
        hwcnt::papi* cpapi = (hwcnt::papi*)papi;
        hwcnt::papi_ctrs_t cstorage = (hwcnt::papi_ctrs_t)storage;
        cpapi->sfree(cstorage);
    }

    void hwcnt_papi_scopy(hwcnt_papi_t papi, hwcnt_papi_ctrs_t dest, hwcnt_papi_ctrs_t src){
        hwcnt::papi* cpapi = (hwcnt::papi*)papi;
        hwcnt::papi_ctrs_t cdest = (hwcnt::papi_ctrs_t)dest;
        hwcnt::papi_ctrs_t csrc = (hwcnt::papi_ctrs_t)src;
        cpapi->scopy(cdest, csrc);
    }

    void hwcnt_papi_read(hwcnt_papi_t papi, hwcnt_papi_ctrs_t storage, int reset){
        hwcnt::papi* cpapi = (hwcnt::papi*)papi;
        hwcnt::papi_ctrs_t cstorage = (hwcnt::papi_ctrs_t)storage;
        cpapi->read(cstorage, reset==0?false:true);
    }

    void hwcnt_papi_fini(hwcnt_papi_t papi, hwcnt_papi_ctrs_t storage){
        hwcnt::papi* cpapi = (hwcnt::papi*)papi;
        hwcnt::papi_ctrs_t cstorage = (hwcnt::papi_ctrs_t)storage;
        cpapi->fini(cstorage);
    }

    void hwcnt_papi_print(hwcnt_papi_t papi, hwcnt_papi_ctrs_t storage){
        hwcnt::papi* cpapi = (hwcnt::papi*)papi;
        hwcnt::papi_ctrs_t cstorage = (hwcnt::papi_ctrs_t)storage;
        cpapi->print(cstorage);
    }

    void hwcnt_papi_add_thread(hwcnt_papi_t papi, pid_t tid){
        hwcnt::papi* cpapi = (hwcnt::papi*)papi;
        cpapi->add_thread(tid);
    }

    uint64_t hwcnt_papi_get_ctr(hwcnt_papi_t papi, hwcnt_papi_ctrs_t storage, uint32_t cpu, uint32_t eventid){
        hwcnt::papi* cpapi = (hwcnt::papi*)papi;
        hwcnt::papi_ctrs_t cstorage = (hwcnt::papi_ctrs_t)storage;
        return cpapi->get_ctr(cstorage, cpu, eventid);
    }

    uint64_t hwcnt_papi_get_ctr_str(hwcnt_papi_t papi, hwcnt_papi_ctrs_t storage, uint32_t cpu, const char* eventname){
        hwcnt::papi* cpapi = (hwcnt::papi*)papi;
        hwcnt::papi_ctrs_t cstorage = (hwcnt::papi_ctrs_t)storage;
        std::string name(eventname);
        return cpapi->get_ctr(cstorage, cpu, name);
    }

    uint32_t hwcnt_papi_event_cnt(hwcnt_papi_t papi){
        hwcnt::papi* cpapi = (hwcnt::papi*)papi;
        return cpapi->event_cnt();
    }

    uint32_t hwcnt_papi_cpu_cnt(hwcnt_papi_t papi){
        hwcnt::papi* cpapi = (hwcnt::papi*)papi;
        return cpapi->cpu_cnt();
    }

};
