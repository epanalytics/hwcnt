/**
 * A thin C layer that sits on top of the hwcnt C++ API
 * 
 * See the hwcnt.hpp to view the API documentation
 */

#ifndef __hwcnt_h__
#define __hwcnt_h__

#ifdef __hwcnt_is_defined__
#error "Cannot include multiple hwcnt headers"
#endif
#define __hwcnt_is_defined__

#include <stdint.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * The C version of the hwcnt::papi object
     *
     * This object is opaque. Only access/manipulate it via the methods in this file
     */
    typedef void* hwcnt_papi_t;

    typedef uint64_t** hwcnt_papi_ctrs_t;

    extern hwcnt_papi_t hwcnt_papi_create(char** events, uint32_t evt_count, pid_t pid, int inherit, int multiplex);
    extern void hwcnt_papi_destroy(hwcnt_papi_t papi);

    extern hwcnt_papi_ctrs_t hwcnt_papi_salloc(hwcnt_papi_t papi);
    extern void hwcnt_papi_sfree(hwcnt_papi_t papi, hwcnt_papi_ctrs_t storage);
    extern void hwcnt_papi_scopy(hwcnt_papi_t papi, hwcnt_papi_ctrs_t dest, hwcnt_papi_ctrs_t src);

    extern void hwcnt_papi_read(hwcnt_papi_t papi, hwcnt_papi_ctrs_t storage, int reset);
    extern void hwcnt_papi_fini(hwcnt_papi_t papi, hwcnt_papi_ctrs_t storage);
    extern void hwcnt_papi_print(hwcnt_papi_t papi, hwcnt_papi_ctrs_t storage);

    extern void hwcnt_papi_add_thread(hwcnt_papi_t papi, pid_t tid);

    extern uint64_t hwcnt_papi_get_ctr(hwcnt_papi_t papi, hwcnt_papi_ctrs_t storage, uint32_t cpu, uint32_t eventid);
    extern uint64_t hwcnt_papi_get_ctr_str(hwcnt_papi_t papi, hwcnt_papi_ctrs_t storage, uint32_t cpu, const char* eventname);

    extern uint32_t hwcnt_papi_event_cnt(hwcnt_papi_t papi);
    extern uint32_t hwcnt_papi_cpu_cnt(hwcnt_papi_t papi);

#ifdef __cplusplus
};
#endif

#endif /* __hwcnt_h__ */
