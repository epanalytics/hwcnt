#ifndef DISABLE_CLTOOLS

#include <stdio.h>
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <assert.h>
#include <string.h>
#include <sys/time.h>
#include <sys/ptrace.h>
#include <errno.h>
#include <fcntl.h>

#include "hwcnt.hpp"

hwcnt::papi* ctr = NULL;
hwcnt::papi_ctrs_t storage;
std::map<uint32_t, pid_t> pid_map;

#define SLEEP_BETWEEN_CHECKS (50000)
#define IDLE_TIME_BEFORE_KILL (500000)

//#define DEBUG_PTRACE
#ifdef DEBUG_PTRACE
#define PTRACE_DEBUG_PRINTF(...) printf(__VA_ARGS__); fflush(stdout)
#else
#define PTRACE_DEBUG_PRINTF(...)
#endif

#define die_error(__msg, ...) fprintf(stderr, "error: " __msg "\n", __VA_ARGS__); exit(-1)
#define HWCNT_MSG "-[hwcnt]-\t"
#define hwcnt_printf(__file, __msg, ...) fprintf(__file, "%s" __msg, HWCNT_MSG, __VA_ARGS__)
FILE* outfile = stdout;
pid_t kill_pid;

#define IS_VFORK_STATUS(__status) (__status>>8 == (SIGTRAP | (PTRACE_EVENT_VFORK<<8)))
#define IS_FORK_STATUS(__status) (__status>>8 == (SIGTRAP | (PTRACE_EVENT_FORK<<8)))
#define IS_CLONE_STATUS(__status) (__status>>8 == (SIGTRAP | (PTRACE_EVENT_CLONE<<8)))
#define IS_EXIT_STATUS(__status) (__status>>8 == (SIGTRAP | (PTRACE_EVENT_EXIT<<8)))
#define IS_VFORKDONE_STATUS(__status) (__status>>8 == (SIGTRAP | (PTRACE_EVENT_VFORK_DONE<<8)))
#define IS_EXEC_STATUS(__status) (__status>>8 == (SIGTRAP | (PTRACE_EVENT_EXEC<<8)))

void print_help(const char* prog){
    printf("usage: %s [options] [-- prog arg1 arg2 ...]\n", prog);
    printf("\toptions\n");
    printf("\t[-h]\t\tprint help message and exit\n");
    printf("\t[-o <file>]\tprint results to <file>\n");
    printf("\t[-c]\t\tCPU mode, tracking events on all CPUs\n");
    printf("\t\t\tif not used, process mode is used to track events for a process\n");
    printf("\t[-s <sep>]\tuse <sep> as a seperator when printing counters (default is tab)\n");
    printf("\t[-p <pid>]\tcount events until <pid> terminates\n");
    printf("\t\t\tif process mode is used, this process is attached to and counted\n");
    printf("\t\t\tif not used, a program and arguments (-- prog arg1 arg2 ...) are expected\n");
    printf("\t[-i]\t\tcount combined events for all children processes/threads spawned, meaningless in CPU mode\n");
    printf("\t[-I]\t\tcount seperate events for all children processes/threads spawned\n");
    printf("\t\t\tcan only be used in process mode (no -c), frequency==0 (-f 0) and non-attach mode (no (-p))\n");
    printf("\t[-a]\t\taccumulate counters\n");
    printf("\t[-x]\t\tmultiplex counters. does nothing in CPU mode\n");
    printf("\t[-f <freq>]\tfrequency of event collection in usec\n");
    printf("\t[-e e1,e2,...]\tevents to collect. use papi_avail/papi_native_avail to find events\n");
    printf("\t\t\tdefault is PAPI_TOT_INS,PAPI_TOT_CYC\n");
}

struct global_timer_t {
private:
    static double ptimer() {
        struct timeval timestr;
        gettimeofday(&timestr, NULL);
        return (double)timestr.tv_sec + 1.0E-06*(double)timestr.tv_usec;
    }
    double t0;
public:
    global_timer_t(){
        t0 = ptimer();
    }
    ~global_timer_t(){}
    float elapsed_time(){
        double t1 = ptimer();
        return float(t1 - t0);
    }
};
global_timer_t* gtimer = NULL;


struct options_t {
    pid_t process;
    bool cpumode;
    int inherit;
    bool multiplex;
    bool accumulate;
    uint64_t frequency;
    std::string seperator;
    std::string ofile;
    std::vector<std::string> events;
    options_t()
        : process(0), cpumode(false), inherit(0), multiplex(false), frequency(0)
    {
        ofile.clear();
        seperator.clear();

        events.clear();
        events.push_back(std::string("PAPI_TOT_INS"));
        events.push_back(std::string("PAPI_TOT_CYC"));
    }

    void set_events(char* evts){
        uint32_t len = strlen(evts)+1;
        char* evts_copy = new char[len];
        strcpy(evts_copy, evts);

        events.clear();
        char* p = strtok(evts_copy, ",");
        while (p){
            events.push_back(std::string(p));
            p = strtok(NULL, ",");
        }

        delete[] evts_copy;
    }

    void print(){
        hwcnt_printf(stdout, "hwcnt-stat options%c\n", ':');
        hwcnt_printf(stdout, "\tofile=%s\n", ofile.size()==0?"stdout":ofile.c_str());
        hwcnt_printf(stdout, "\tseperator=%s\n", seperator.c_str());
        hwcnt_printf(stdout, "\tprocess=%d\n", process);
        hwcnt_printf(stdout, "\tcpumode=%d\n", cpumode);
        hwcnt_printf(stdout, "\tinherit=%d\n", inherit);
        hwcnt_printf(stdout, "\taccumulate=%d\n", accumulate);
        hwcnt_printf(stdout, "\tmultiplex=%d\n", multiplex);
        hwcnt_printf(stdout, "\tfrequency=%lu\n", frequency);
        hwcnt_printf(stdout, "\tevents=%s", events[0].c_str());
        for (uint32_t i = 1; i < events.size(); ++i){
            fprintf(stdout, ",%s", events[i].c_str());
        }
        fprintf(stdout, "\n");
    }
};

uint64_t string_to_uint64(std::string inp){
    const char* cinp = inp.c_str();
    char* endptr = NULL;
    long int res = strtol(cinp, &endptr, 0);
    if (*endptr == cinp[0]){
        die_error("cannot convert string to int (%s)", cinp);
    }
    return uint64_t(res);
}

void kill_em_all(){
    fprintf(stderr, "Cleaning up process: %d\n", kill_pid);
    kill(kill_pid, SIGKILL);
}

void flush_files(int signo){
    if (ctr) ctr->fini(storage);
    ctr->sfree(storage);
    delete ctr;

    fflush(outfile);
    kill_em_all();
    exit(signo);
}

void print_header(const char* seperator, bool cpumode){
    uint32_t nevts = ctr->event_cnt();

    if (outfile == stdout || outfile == stderr){
        fprintf(outfile, HWCNT_MSG);
    }

    fprintf(outfile, "iteration%s%s%stimer", seperator, cpumode?"cpu":"pid", seperator);
    for (uint32_t evt = 0; evt < nevts; ++evt){
        fprintf(outfile, "%s%s", seperator, ctr->event_name(evt).c_str());
    }
    fprintf(outfile, "\n");
}

uint64_t iteration = 0;
void print_counters(const char* seperator, pid_t track_pid, bool cpumode){
    uint32_t ncpus = ctr->cpu_cnt();
    uint32_t nevts = ctr->event_cnt();

    if (iteration == 0){
        print_header(seperator, cpumode);
    }

    float etime = gtimer->elapsed_time();
    for (uint32_t cpu = 0; cpu < ncpus; ++cpu){
        if (outfile == stdout || outfile == stderr){
            fprintf(outfile, HWCNT_MSG);
        }
        fprintf(outfile, "%lu%s%u%s%.4f", iteration, seperator, cpumode?cpu:(cpu==0?track_pid:pid_map[cpu]), seperator, etime);
        for (uint32_t evt = 0; evt < nevts; ++evt){
            fprintf(outfile, "%s%lu", seperator, ctr->get_ctr(storage, cpu, evt));
        }
        fprintf(outfile, "\n");
    }

    iteration++;
}

int main(int argc, char** argv){
    int c;
    options_t opt;
    bool needs_cleanup = false;

#define str_set_once(__str, __value)\
    if (__str.size() > 0){ print_help(argv[0]); die_error("cannot pass option twice (-%c)", c); } \
    __str.append(__value);

    while (-1 != (c = getopt(argc, argv, "ho:s:p:ciIxaf:e:"))){
        switch (c){
        case 'h':
            print_help(argv[0]);
            exit(0);
            break;
        case 'p':
            opt.process = (pid_t)string_to_uint64(std::string(optarg));
            if (opt.process <= 0){
                print_help(argv[0]);
                die_error("a positive integral argument is expected to -p, you passed %s", optarg);
            }
            break;
        case 'c':
            opt.cpumode = true;
            break;
        case 's':
            str_set_once(opt.seperator, optarg);
            break;
        case 'o':
            str_set_once(opt.ofile, optarg);
            break;
        case 'i':
            if (opt.inherit > 0){
                print_help(argv[0]);
                die_error("use either %s or %s", "-i", "-I");
            }
            opt.inherit = 1;
            break;
        case 'I':
            if (opt.inherit > 0){
                print_help(argv[0]);
                die_error("use either %s or %s", "-i", "-I");
            }
            opt.inherit = 2;
            break;
        case 'x':
            opt.multiplex = true;
            break;
        case 'a':
            opt.accumulate = true;
            break;
        case 'f':
            opt.frequency = string_to_uint64(std::string(optarg));
            break;
        case 'e':
            opt.set_events(optarg);
            break;
        default:
            print_help(argv[0]);
            die_error("unexpected option -%c given", c);
        }

    }

    if (opt.inherit == 2){
        if (opt.process > 0 || opt.cpumode == true || opt.frequency > 0){
            print_help(argv[0]);
            die_error("You supplied %s. This can only be used with %s, without %s, and without %s", "-I", "-f 0", "-c", "-p <pid>");
        }
    }

    if (opt.ofile.size() > 0){
        outfile = fopen(opt.ofile.c_str(), "w");
        if (outfile == NULL){
            print_help(argv[0]);
            die_error("cannot open file: %s", opt.ofile.c_str());
        }
    }

    if (outfile == stdout || outfile == stderr){
        fprintf(outfile, HWCNT_MSG);
    }
    fprintf(outfile, "# command used: %s", argv[0]);
    for (int i = 1; i < argc; ++i){
        fprintf(outfile, " %s", argv[i]);
    }
    fprintf(outfile, "\n");


    if (opt.seperator.size() == 0){
        opt.seperator.append("\t");
    }

    //opt.print();

    const char* seperator = opt.seperator.c_str();

    pid_t track_pid = opt.process;


    if (opt.cpumode){
        gtimer = new global_timer_t();
        ctr = new hwcnt::papi(opt.events, 0, false, opt.multiplex);
    }

    if (track_pid){

        if (optind != argc){
            print_help(argv[0]);
            fprintf(stderr, "warn: extra arguments: ");
            for (int i = optind; i < argc; i++){
                fprintf(stderr, "%s ", argv[i]);
            }
            fprintf(stderr, "\n");
            die_error("Extra arguments detected in process attach mode (%s)", "-p");
        }

    } else {

        track_pid = fork();
        if (track_pid == 0){

            // TODO: seems to wipe out signal handing in some cases
            ptrace(PTRACE_TRACEME, 0, NULL, NULL);

            int ret = execvp(argv[optind], &(argv[optind]));
            fprintf(stderr, "cannot execute program: ");
            for (int i = optind; i < argc; i++){
                fprintf(stderr, "%s ", argv[i]);
            }
            fprintf(stderr, "\n");
            die_error("execvp failed with code %d", ret);
        }

        kill_pid = track_pid;
    }

    if (!opt.cpumode){
        gtimer = new global_timer_t();
        ctr = new hwcnt::papi(opt.events, track_pid, opt.inherit == 1, opt.multiplex);
    }

    signal(SIGINT, flush_files);
    signal(SIGTERM, flush_files);

    assert(NULL != ctr && "ctr should be initialized");
    storage = ctr->salloc();

    int status;
    if (opt.frequency > 0){
        if (opt.process == 0){
            wait(NULL);
            ptrace(PTRACE_CONT, track_pid, 0, 0);
        }

        while (waitpid(track_pid, &status, WNOHANG) == 0){
            ctr->read(storage, !opt.accumulate);
            print_counters(seperator, track_pid, opt.cpumode);
            usleep(opt.frequency);
        }
    } else {

        if (opt.inherit == 2 && opt.process == 0){

            wait(NULL);
            ptrace(PTRACE_SETOPTIONS, track_pid, 0, PTRACE_O_TRACEVFORK | PTRACE_O_TRACEFORK | PTRACE_O_TRACECLONE | PTRACE_O_TRACEEXIT | PTRACE_O_TRACEVFORKDONE | PTRACE_O_TRACEEXEC);
            ptrace(PTRACE_CONT, track_pid, 0, 0);

            #ifdef DEBUG_PTRACE
            int count = 0;
            #endif
            int child_count = 0;
            bool has_children = false;
            uint64_t idle_count = 0;

            PTRACE_DEBUG_PRINTF("main process is %d\n", track_pid);

            do {
                int status;
                PTRACE_DEBUG_PRINTF("controller waiting on all...\n");
                pid_t npid = waitpid(-1, &status, __WALL | WNOHANG);
                if (npid == -1){
                    assert(false && "Error in waitpid");
                    continue;
                } else if (npid == 0){
                    PTRACE_DEBUG_PRINTF("controller sleeping...\n");

                    // HACK: a process with dead children that is idle for a long time is assumed to be dead.
                    //       certain mpirun implementations have a parent process that apparently never exits...
                    if (has_children && child_count == 0 && idle_count >= IDLE_TIME_BEFORE_KILL){
                        needs_cleanup = true;
                        break;
                    }
                    idle_count += SLEEP_BETWEEN_CHECKS;
                    usleep(SLEEP_BETWEEN_CHECKS);
                    continue;
                }
                idle_count = 0;

                PTRACE_DEBUG_PRINTF("signal with pid %d found %d -- ", npid, ++count);
                if (WIFCONTINUED(status)){
                    PTRACE_DEBUG_PRINTF("child continued\n");
                }
                else if (WIFEXITED(status)){
                    child_count--;
                    PTRACE_DEBUG_PRINTF("child exited. %d remaining\n", child_count);
                    if (npid == track_pid){
                        assert(child_count == -1 && "Parent exited without all children exiting??");
                        break;
                    }
                }
                else if (WIFSTOPPED(status)){
                    if (IS_VFORK_STATUS(status) || IS_FORK_STATUS(status) || IS_CLONE_STATUS(status)){
                        unsigned long data;
                        ptrace(PTRACE_GETEVENTMSG, track_pid, 0, &data);
                        PTRACE_DEBUG_PRINTF("parent %s'd to %lu\n", IS_VFORK_STATUS(status)?"VFORK":(IS_FORK_STATUS(status)?"FORK":"CLONE"), data);
                        pid_map[ctr->cpu_cnt()] = pid_t(data);
                        ctr->add_thread(data);
                        child_count++;
                        has_children = true;
                    } else if (IS_EXIT_STATUS(status)){
                        PTRACE_DEBUG_PRINTF("parent exited\n");
                    } else if (IS_VFORKDONE_STATUS(status)){
                        PTRACE_DEBUG_PRINTF("parent vforkdone\n");
                    } else if (IS_EXEC_STATUS(status)){
                        PTRACE_DEBUG_PRINTF("parent exec\n");
                    } else {
                        #ifdef DEBUG_PTRACE
                        int signo = WSTOPSIG(status);
                        #endif
                        PTRACE_DEBUG_PRINTF("parent stopped %d\n", signo);
                    }
                }
                else {
                    PTRACE_DEBUG_PRINTF("child other\n");
                }
                fflush(stdout);

                ptrace(PTRACE_CONT, npid, NULL, NULL);
            } while (1);
        } else {
            if (opt.process == 0){
                wait(NULL);
                ptrace(PTRACE_CONT, track_pid, 0, 0);
            }
            waitpid(track_pid, &status, 0);
        }
    }

    ctr->fini(storage);
    print_counters(seperator, track_pid, opt.cpumode);

    ctr->sfree(storage);

    if (opt.ofile.size() > 0){
        fclose(outfile);
    }

    delete gtimer;

    // TODO: collect child output and flush it before returning
    fflush(stdout);
    fflush(stderr);

    if (needs_cleanup){
        kill_em_all();
    }

    return 0;
}

#else // DISABLE_CLTOOLS
#include <stdio.h>

int main(int argc, char** argv){
    fprintf(stderr, "This package was configured with --disable-cltools. Aborting!\n");
    return -1;
}

#endif // DISABLE_CLTOOLS
