#ifndef __hwcnt_hpp__
#define __hwcnt_hpp__

#ifdef __hwcnt_is_defined__
#error "Cannot include multiple hwcnt headers"
#endif
#define __hwcnt_is_defined__

#include <stdint.h>
#include <unistd.h>

#include <vector>
#include <set>
#include <string>
#include <map>

namespace hwcnt {

    /**
     * papi_ctrs_t is an opaque data structure holding just the counter values.
     * Its definition may change in later versions.
     * 
     * It should not be malloc'ed/new'ed directly. Instead use the salloc member function.
     * Similarly, free it with the sfree member function.
     *
     * It should not be modified or accessed directly. Instead use the read/fini/get_ctr
     * member functions.
     */
    typedef uint64_t** papi_ctrs_t;

    class papi {
    private:
        std::vector<std::string> events;
        std::map<std::string, uint32_t> event_name_to_id;

        int* EventSets;
        uint32_t ncpus;
        pid_t process_only;

        std::set<uint64_t> allocated;

        void set_ctr(papi_ctrs_t storage, uint32_t cpu, uint32_t eventid, uint64_t val);
        bool is_alloced(papi_ctrs_t storage);

    public:

        /**
         * All of the interesting configuration happens in this constructor.
         *
         * @param evts string containing the names of events to collect. use papi_avail and
         *     papi_native_avail to determine which events are supported on your platform
         * @param pid if set to 0, events are monitored on all CPUs ("CPU mode"). if non-zero, 
         *     events for a particular process are collected ("process mode")
         * @param inherit nop in CPU mode. in process mode, setting inherit to true will collect
         *     events on process pid and all its children processes/threads spawned in the future
         * @param multiplex set to true to enable multiplexing
         */
        papi(std::vector<std::string>& evts, pid_t pid, bool inherit, bool multiplex);

        /**
         * destructor
         */
        ~papi();

        /**
         * allocate an opaque object to store counters
         * @return a counter storage object
         */
        papi_ctrs_t salloc();

        /**
         * free a counter storage object
         * @param storage a counter storage object
         * @return none
         */
        void sfree(papi_ctrs_t storage);

        /**
         * copy contents of one counter storage object to another
         * @param dest the destination counter storage object
         * @param src the source counter storage object
         * @return none
         */
        void scopy(papi_ctrs_t dest, papi_ctrs_t src);

        /**
         * read counters into a counter storage object
         * @param storage a counter storage object
         * @param reset reset counters to zero iff reset==true. thus, reset==false accumulates counters
         * @return none
         */
        void read(papi_ctrs_t storage, bool reset);

        /**
         * finalize counting, reading counters one final time
         * @param storage a counter storage object
         * @return none
         */
        void fini(papi_ctrs_t storage);

        /**
         * print counters
         * @param storage a counter storage object
         * @return none
         */
        void print(papi_ctrs_t storage);

        /**
         * attach and collect counter for a new thread. invalid to call this method in CPU mode
         * @param tid the id of the new thread/process
         * @return none
         */
        void add_thread(pid_t tid);

        /**
         * get the value of a single counter
         * @param storage a counter storage object
         * @param cpu the CPU of the counter to get. must be 0 in process mode
         * @param eventid the index of the event to get
         * @return the value of the counter in storage for CPU cpu and event index eventid
         */
        uint64_t get_ctr(papi_ctrs_t storage, uint32_t cpu, uint32_t eventid);

        /**
         * get the value of a single counter. Note that the string-less interface is faster
         * @param storage a counter storage object
         * @param cpu the CPU of the counter to get. must be 0 in process mode
         * @param eventname the name of the event
         * @return the value of the counter in storage for CPU cpu and event name eventname
         */
        uint64_t get_ctr(papi_ctrs_t storage, uint32_t cpu, std::string& eventname);

        /**
         * get the count of events being collected. this is identical to the number of events passed into the constructor
         * @return the count of events
         */
        uint32_t event_cnt();

        /**
         * get the count of cpus for events
         * @return the count of cpus. this is 1 in process mode, and is number of CPUs on the system in CPU mode
         */
        uint32_t cpu_cnt();

        /**
         * get the name of an event
         * @param idx the index of the event
         * @return the name of the event with index idx
         */
        std::string& event_name(uint32_t idx);
    };
};

#endif /* __hwcnt_hpp__ */
