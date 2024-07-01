#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <list>
#include <algorithm>
#include <unistd.h>
#include <string>

#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)
#define PATH_MAX (1024)

class Command
{
    // TODO: Add your data members
public:
    char *cmd_line;
    char **args;
    int num_args;

public:
    // Command() = default;
    Command(const char *cmd_line);
    ~Command();
    virtual void execute() = 0;
    // virtual void prepare();
    // virtual void cleanup();
    //  TODO: Add your extra methods if needed
};

class BuiltInCommand : public Command
{
public:
    BuiltInCommand(const char *cmd_line) : Command(cmd_line) {}
};

class ExternalCommand : public Command
{
public:
    ExternalCommand(const char *cmd_line); // in cpp
    // virtual ~ExternalCommand() {}
    void execute() override;
};

enum PipeType
{
    Fromstdout,
    Fromstderr
};
class PipeCommand : public Command
{
    // TODO: Add your data members
    PipeType type;

public:
    PipeCommand(const char *cmd_line, PipeType type) : Command(cmd_line), type(type) {}
    // virtual ~PipeCommand() {}
    void execute() override;
};

enum RedirectType
{
    Override,
    Append
};

class RedirectionCommand : public Command
{
    // TODO: Add your data members
    RedirectType type;

public:
    explicit RedirectionCommand(const char *cmd_line, RedirectType type) : Command(cmd_line), type(type) {}
    // virtual ~RedirectionCommand() {}
    void execute() override;
    // void prepare() override;
    // void cleanup() override;
};

class ChangePromptCommand : public BuiltInCommand
{ // V
  // TODO: Add your data members public:
public:
    ChangePromptCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}
    // virtual ~ChangePromptCommand() {}
    void execute() override;
};

class ChangeDirCommand : public BuiltInCommand
{ // cd
  // TODO: Add your data members public:
public:
    ChangeDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}
    // virtual ~ChangeDirCommand() {}
    void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand
{ // pwd
public:
    GetCurrDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}
    // virtual ~GetCurrDirCommand() {}
    void execute() override;
};

class ShowPidCommand : public BuiltInCommand
{ // V showpid
public:
    ShowPidCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}
    // virtual ~ShowPidCommand() {}
    void execute() override;
};

class JobsList;
class QuitCommand : public BuiltInCommand
{
    // TODO: Add your data members
public:
    QuitCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}
    // virtual ~QuitCommand() {}
    void execute() override;
};

class JobEntry
{
    // TODO: Add your data members
public:
    pid_t PID;
    int jobID;
    const char *command;
    time_t startTime;
    Command *cmnd;

    bool isStopped;
    bool isBackground;

    // bool isComplex
    friend class JobStack;

public:
    JobEntry(int jobID, pid_t PID, const char *command, time_t startTime, Command *cmnd, bool isStopped = false);
};

class JobStack : public std::list<JobEntry *>
{
public:
    JobStack() : std::list<JobEntry *>() {}
    ~JobStack()
    {
        while (!empty())
        {
            JobEntry *entry = back();
            pop_back();
            delete entry;
        }
    }
};

class JobsList
{
public:
    // TODO: Add your data members
    // JobStack* StoppedJobs;
    JobStack *jobsList;
    int maxJobID;

public:
    JobsList() : jobsList(new JobStack()), maxJobID(1) {}
    ~JobsList(); // default
    void addJob(Command *cmd, pid_t job_pid, bool isStopped = false);
    void printJobsList();
    void killAllJobs();
    void removeFinishedJobs();
    JobEntry *getJobById(int jobId);
    JobEntry *getJobByPID(int PID);
    void removeJobById(int jobId);
    JobEntry *getLastJob(int *lastJobId);
    JobEntry *getLastStoppedJob(int *jobId);
    // TODO: Add extra methods or modify exisitng ones as needed
};

// class DirsList {
// public:
//     DirStack* dirStack;
//     int maxDirID;
//
//     DirsList() : dirStack(), maxDirID(1);
//     ~DirsList();
//     void addDir(std::string path){
//         if(path.length() > PATH_MAX){
//             return;
//         }
//         this->dirStack->push(new Directory(maxDirID,path));
//     }
//
// };

class JobsCommand : public BuiltInCommand
{
    // TODO: Add your data members
public:
    JobsCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}
    // virtual ~JobsCommand() {}
    void execute() override;
};

class ForegroundCommand : public BuiltInCommand
{
    // TODO: Add your data members
public:
    ForegroundCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}
    // virtual ~ForegroundCommand() {}
    void execute() override;
};

class BackgroundCommand : public BuiltInCommand
{
    // TODO: Add your data members
public:
    BackgroundCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}
    // virtual ~BackgroundCommand() {}
    void execute() override;
};

class TimeoutCommand : public BuiltInCommand
{
    /* Bonus */
    // TODO: Add your data members
public:
    explicit TimeoutCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}
    // virtual ~TimeoutCommand() {}
    void execute() override;
};

class ChmodCommand : public BuiltInCommand
{
    // TODO: Add your data members
public:
    ChmodCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}
    // virtual ~ChmodCommand() {}
    void execute() override;
};

class GetFileTypeCommand : public BuiltInCommand
{
    // TODO: Add your data members
public:
    GetFileTypeCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}
    // virtual ~GetFileTypeCommand() {}
    void execute() override;
};

class SetcoreCommand : public BuiltInCommand
{
    // TODO: Add your data members
public:
    SetcoreCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}
    // virtual ~SetcoreCommand() {}
    void execute() override;
};

class KillCommand : public BuiltInCommand
{
    // TODO: Add your data members
public:
    KillCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}
    // virtual ~KillCommand() {}
    void execute() override;
};

enum executeType
{
    Normal,
    Redirection
};

class SmallShell
{
public:
    // TODO: Add your data members

    std::string prompt;
    JobsList *jobsList;
    // std::string curr_dir;
    std::string last_dir;
    bool eventDirectoryHasChanged;
    pid_t shell_PID;
    pid_t foreground_pid;
    Command *foreground_command;
    // pid_t command_pid; Still unsure if neccesary, initalize to 0 if it is
    SmallShell() : prompt("smash"), jobsList(new JobsList()), last_dir(""), eventDirectoryHasChanged(false), shell_PID(getpid()), foreground_pid(-1) {}

public:
    Command *CreateCommand(const char *cmd_line);
    SmallShell(SmallShell const &) = delete;     // disable copy ctor
    void operator=(SmallShell const &) = delete; // disable = operator
    std::string getPrompt()
    {
        return prompt;
    }
    static SmallShell &getInstance() // make SmallShell singleton
    {
        static SmallShell instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }
    ~SmallShell();
    void UpdateForeground(Command *command, pid_t pid);
    void executeCommand(const char *cmd_line);
    // TODO: add extra methods as needed
};

#endif // SMASH_COMMAND_H_
