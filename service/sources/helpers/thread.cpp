#include <pthread.h>
#include <signal.h>
#include "helpers/thread.h"

namespace SocialNetwork {

namespace ThreadHelpers {

void block_signals()
{
    // man signals(7)
    //
    //    Signal        x86/ARM     Alpha/   MIPS   PARISC   Same as    Action  Notes
    //                most others   SPARC
    //    ─────────────────────────────────────────────────────────────────────────────
    //    SIGHUP           1           1       1       1                Term    Hangup detected on controlling terminal
    //    SIGINT           2           2       2       2                Term    Interrupt from keyboard (Ctrl+C)
    //    SIGQUIT          3           3       3       3                Core    Quit from keyboard (Ctrl+\)
    //    SIGILL           4           4       4       4                Core    Illegal instruction
    //    SIGTRAP          5           5       5       5                Core    Trace/breakpoint trap
    //    SIGABRT          6           6       6       6     SIGIOT     Core    Abnormal termination, abort()
    //    SIGBUS           7          10      10      10                Core    Bus error (bad memory access)
    //    SIGEMT           -           7       7      -                 Term    Emulator trap
    //    SIGFPE           8           8       8       8                Core    Erroneous arithmetic operation
    //    SIGKILL          9           9       9       9                Term    Kill signal
    //    SIGUSR1         10          30      16      16                Term    User-defined signal 1
    //    SIGSEGV         11          11      11      11                Core    Invalid memory reference
    //    SIGUSR2         12          31      17      17                Term    User-defined signal 2
    //    SIGPIPE         13          13      13      13                Term    Broken pipe
    //    SIGALRM         14          14      14      14                Term    Alarm clock, alarm()
    //    SIGTERM         15          15      15      15                Term    Termination signal
    //    SIGSTKFLT       16          -       -        7                Term    Stack fault on coprocessor (obsolete)
    //    SIGCHLD         17          20      18      18     SIGCLD     Ign     Child terminated or stopped
    //    SIGCONT         18          19      25      26                Cont    Continue if stopped
    //    SIGSTOP         19          17      23      24                Stop    Stop process
    //    SIGTSTP         20          18      24      25                Stop    Keyboard stop (Ctrl+Z)
    //    SIGTTIN         21          21      26      27                Stop    Background read from control terminal
    //    SIGTTOU         22          22      27      28                Stop    Background write to control terminal
    //    SIGURG          23          16      21      29                Ign     Urgent data is available at a socket
    //    SIGXCPU         24          24      30      12                Core    CPU time limit exceeded
    //    SIGXFSZ         25          25      31      30                Core    File size limit exceeded
    //    SIGVTALRM       26          26      28      20                Term    Virtual timer expired
    //    SIGPROF         27          27      29      21                Term    Profiling timer expired
    //    SIGWINCH        28          28      20      23                Ign     Window size change
    //    SIGPOLL         29          23      22      22     SIGIO      Term    Pollable event occurred
    //    SIGPWR          30         29/-     19      19                Term    Power failure imminent
    //    SIGINFO          -         29/-     -       -      SIGPWR     Term
    //    SIGLOST          -         -/29     -       -      SIGPWR     Term
    //    SIGSYS          31          12      12      31                Core    Bad system call

    // сигнал SIGKILL не может быть перехвачен, заблокирован или игнорирован. ядро всегда завершает процесс
    // сигнал SIGSTOP не может быть перехвачен, заблокирован или игнорирован. ядро всегда останавливает процесс
    // сигнал SIGCONT особый сигнал, если процесс остановлен, то ядро всегда сначала возобновляет процесс
    // сигналы SIGBUS, SIGFPE, SIGILL, SIGSEGV - аппаратно генерируемые. их не стоит игнорировать, блокировать
    //      или создавать для них обработчик, из которого процесс нормально возвращается, т.к. процесс вернется
    //      ровно в ту инструкцию, которая вызвала аппаратную проблему. тут нужно либо смириться с действием
    //      по умолчанию (завершение процесса), либо выйти из обработчика через _exit(), либо делать siglongjump()

    sigset_t sset;
    sigemptyset(&sset);
    sigaddset(&sset, SIGQUIT);
    sigaddset(&sset, SIGTERM);
    sigaddset(&sset, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &sset, 0);
}

void set_name(pthread_t th, const std::string& name)
{
    static const size_t max_name_length = 15;
    std::string real_name(name);
    if (name.size() > max_name_length) {
        int half = (max_name_length - 1) / 2;
        std::string truncated(name, 0, half);
        truncated.append("~");
        truncated.append(name, name.size() - half);
        real_name = truncated;
    }

    pthread_setname_np(th, real_name.c_str());
}

std::string get_name(pthread_t th)
{
    static const size_t max_name_length = 15;
    char name[max_name_length + 1] = {0};
    pthread_getname_np(th, name, sizeof(max_name_length));
    name[max_name_length] = 0;
    return std::string(name);
}

} // namespace ThreadHelpers

} // namespace SocialNetwork
