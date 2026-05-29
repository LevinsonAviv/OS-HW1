#include <unistd.h>
#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlCHandler(int sig_num) { // not async safe
    pid_t foregroundPid = SmallShell::getInstance().getForegroundPid();
    if(foregroundPid == -1){
        return;
    }
    string signalMsg = "smash: got ctrl-C\n";
    write(STDOUT_FILENO, signalMsg.c_str(), signalMsg.length());
    string killMsg = "process " + to_string(foregroundPid) + " was killed\n";
    write(STDOUT_FILENO, killMsg.c_str(), killMsg.length());
    kill(foregroundPid, SIGKILL);
    SmallShell::getInstance().setForegroundPid(-1);
}
