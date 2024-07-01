#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlZHandler(int sig_num)
{
    std::cout << "smash: got ctrl-Z" << std::endl;
    SmallShell &smash = SmallShell::getInstance();
    // Check if there is a foreground job
    pid_t Fpid = smash.foreground_pid;
    if (Fpid == -1)
    {
        // no job, return
        return;
    }
    if (kill(Fpid, SIGSTOP) == -1)
    {
        perror("smash error: kill failed");
        return;
    }
    JobEntry *existing_job = smash.jobsList->getJobByPID(Fpid);
    if (existing_job != nullptr)
    {
        // Job exists already, update it to Stopped
        existing_job->isBackground = false;
        existing_job->isStopped = true;
    }
    else
    {
        smash.jobsList->addJob(smash.foreground_command,
                               Fpid, true);
    }
    std::cout << "smash: process " << Fpid << " was stopped" << std::endl;
    smash.UpdateForeground(nullptr, -1);
    return;
}

void ctrlCHandler(int sig_num)
{
    std::cout << "smash: got ctrl-C" << std::endl;
    SmallShell &smash = SmallShell::getInstance();
    // Check if there is a foreground job
    pid_t Fpid = smash.foreground_pid;
    if (Fpid == -1)
    {
        // no job, return
        return;
    }
    if (kill(Fpid, SIGKILL) == -1)
    {
        perror("smash error: kill failed");
        return;
    }

    std::cout << "smash: process " << Fpid << " was killed" << std::endl;
    if (smash.jobsList->getJobByPID(Fpid) != nullptr)
    {
        smash.jobsList->removeJobById(smash.jobsList->getJobByPID(Fpid)->jobID);
    }
    //    try{
    //        smash.jobsList->jobsList->removeJobByID(smash.jobsList->getJobByPID(Fpid)->jobID);
    //    } catch(...){
    //        // do nothing
    //    }
    smash.UpdateForeground(nullptr, -1);
    // exit(EXIT_FAILURE);
    return;
}

void alarmHandler(int sig_num)
{
    // TODO: Add your implementation
}
