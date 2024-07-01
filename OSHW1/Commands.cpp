#include "Commands.h"
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>
#include <map> // made changes here
#include <regex>
#include <dirent.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <chrono>
#include <thread>
// newest
using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";

#if 0
#define FUNC_ENTRY() cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT() cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

#define DO_SYS(syscall)                                 \
    do                                                  \
    {                                                   \
        if ((syscall) == -1)                            \
        {                                               \
            perror("smash error: " #syscall " failed"); \
        }                                               \
    } while (0)

string _ltrim(const std::string &s)
{
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string &s)
{
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string &s) { return _rtrim(_ltrim(s)); }

int _parseCommandLine(const char *cmd_line, char **args)
{
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for (std::string s; iss >> s;)
    {
        args[i] = (char *)malloc(s.length() + 1);
        memset(args[i], 0, s.length() + 1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;

    FUNC_EXIT()
}

bool _isBackgroundComamnd(const char *cmd_line)
{
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char *cmd_line)
{
    const string str(cmd_line);
    // find last character other than spaces
    unsigned int idx = str.find_last_not_of(WHITESPACE);
    // if all characters are spaces then return
    if (idx == string::npos)
    {
        return;
    }
    // if the command line does not end with & then return
    if (cmd_line[idx] != '&')
    {
        return;
    }
    // replace the & (background sign) with space and then remove all tailing
    // spaces.
    cmd_line[idx] = ' ';
    // truncate the command line string up to the last non-space character
    cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

// Alias data structure
map<string, string> aliases; // made changes here
list<string> aliasOrder;

// Helper function to check if a string is a reserved keyword
bool isReservedKeyword(const string &name)
{ // made changes here
    static const vector<string> reservedKeywords = {"alias", "unalias", ">"};
    return find(reservedKeywords.begin(), reservedKeywords.end(), name) != reservedKeywords.end();
}

// Function to add an alias
void addAlias(const string &name, const string &command)
{
    if (isReservedKeyword(name))
    {
        cerr << "smash error: alias: " << name << " already exists or is a reserved command " << endl;
        return;
    }
    // Check if alias already exists
    if (aliases.find(name) == aliases.end())
    {
        // New alias, add to map and order list
        aliases[name] = command;
        aliasOrder.push_back(name);
    }
    else
    {
        // Existing alias, update command
        // aliases[name] = command;
        cerr << "smash error: alias: " << name << " already exists or is a reserved command " << endl;
        return;
    }
}

// Function to print all aliases in insertion order
void printAliases()
{
    for (const string &name : aliasOrder)
    {
        cout << name << "='" << aliases[name] << "'" << endl;
    }
}

// Alias command implementation
void handleAliasCommand(const vector<string> &args, const string &original_cmd_line)
{
    if (args.size() == 1)
    {
        // List all aliases
        printAliases();
        return;
    }

    // Combine all args into a single string (in case there are spaces in the command part)
    string aliasDefinition;
    for (size_t i = 1; i < args.size(); ++i)
    {
        if (i > 1)
            aliasDefinition += " ";
        aliasDefinition += args[i];
    }

    std::regex aliasRegex(R"((\w+)='(.*)')");
    std::smatch match;
    if (!std::regex_match(aliasDefinition, match, aliasRegex) || match.size() != 3)
    {
        cout << "smash error: alias: invalid alias format" << endl;
        return;
    }

    string name = match[1].str();
    string command = match[2].str();

    addAlias(name, command);
}

// Unalias command implementation
void handleUnaliasCommand(const vector<string> &args)
{
    if (args.size() < 2)
    {
        cerr << "smash error: unalias: not enough arguments" << endl;
        return;
    }

    for (size_t i = 1; i < args.size(); ++i)
    {
        const string &name = args[i];
        auto it = aliases.find(name);
        if (it != aliases.end())
        {
            aliases.erase(it);
            aliasOrder.remove(name);
        }
        else
        {
            cerr << "smash error: unalias: " << name << " alias does not exist" << endl;
            return;
        }
    }
}

class ListDirCommand : public Command
{
public:
    ListDirCommand(const char *cmd_line) : Command(cmd_line) {}
    void execute() override
    {
        if (num_args > 2)
        {
            std::cerr << "smash error: listdir: too many arguments" << std::endl;
            return;
        }

        const char *path = (num_args == 1) ? "." : args[1];

        DIR *dir = opendir(path);
        if (!dir)
        {
            perror("smash error: opendir failed");
            return;
        }

        std::vector<std::string> files;
        std::vector<std::string> dirs;
        std::vector<std::string> links;

        struct dirent *entry;
        while ((entry = readdir(dir)) != nullptr)
        {
            std::string name(entry->d_name);
            if (entry->d_type == DT_REG)
            {
                files.push_back("file: " + name);
            }
            else if (entry->d_type == DT_DIR)
            {
                dirs.push_back("directory: " + name);
            }
            else if (entry->d_type == DT_LNK)
            {
                links.push_back("link: " + name);
            }
        }
        closedir(dir);

        std::sort(files.begin(), files.end());
        std::sort(dirs.begin(), dirs.end());
        std::sort(links.begin(), links.end());

        for (const std::string &file : files)
        {
            std::cout << file << std::endl;
        }
        for (const std::string &dir : dirs)
        {
            std::cout << dir << std::endl;
        }
        for (const std::string &link : links)
        {
            std::cout << link << std::endl;
        }
    }
};
class GetUserCommand : public Command
{
public:
    GetUserCommand(const char *cmd_line) : Command(cmd_line) {}
    void execute() override
    {
        if (num_args != 2)
        {
            std::cerr << "smash error: getuser: too many arguments" << std::endl;
            return;
        }

        pid_t pid;
        try
        {
            pid = std::stoi(args[1]);
        }
        catch (...)
        {
            std::cerr << "smash error: getuser: process " << args[1] << " does not exist" << std::endl;
            return;
        }

        // Get the process user and group
        std::string proc_path = "/proc/" + std::to_string(pid) + "/status";
        FILE *proc_file = fopen(proc_path.c_str(), "r");
        if (!proc_file)
        {
            std::cerr << "smash error: getuser: process " << pid << " does not exist" << std::endl;
            return;
        }

        char line[256];
        uid_t uid = -1;
        gid_t gid = -1;
        while (fgets(line, sizeof(line), proc_file))
        {
            if (strncmp(line, "Uid:", 4) == 0)
            {
                sscanf(line, "Uid: %u", &uid);
            }
            if (strncmp(line, "Gid:", 4) == 0)
            {
                sscanf(line, "Gid: %u", &gid);
            }
        }
        fclose(proc_file);

        if (uid == (uid_t)-1 || gid == (gid_t)-1)
        {
            std::cerr << "smash error: getuser: process " << pid << " does not exist" << std::endl;
            return;
        }

        struct passwd *pwd = getpwuid(uid);
        struct group *grp = getgrgid(gid);
        if (!pwd || !grp)
        {
            std::cerr << "smash error: getuser: process " << pid << " does not exist" << std::endl;
            return;
        }

        std::cout << "User: " << pwd->pw_name << std::endl;
        std::cout << "Group: " << grp->gr_name << std::endl;
    }
};

void SmallShell::executeCommand(const char *cmd_line)
{
    string cmd_str = string(cmd_line);
    string original_cmd_str = cmd_str;

    istringstream iss(cmd_str);
    vector<string> args;
    for (string s; iss >> s;)
    {
        args.push_back(s);
    }

    if (args.empty())
    {
        return;
    }

    const string &command = args[0];

    if (command == "alias")
    {
        handleAliasCommand(args, cmd_str);
        return;
    }
    else if (command == "unalias")
    {
        handleUnaliasCommand(args);
        return;
    }

    // Check if the command is an alias and substitute it
    auto aliasIt = aliases.find(command);
    if (aliasIt != aliases.end())
    {
        string new_cmd_line = aliasIt->second;
        for (size_t i = 1; i < args.size(); ++i)
        {
            new_cmd_line += " " + args[i];
        }
        cmd_line = new_cmd_line.c_str();
    }

    Command *cmd = CreateCommand(cmd_line);
    if (cmd != nullptr)
    {
        cmd->execute();
    }
}

SmallShell::~SmallShell()
{
    // TODO: add your implementation
}

JobEntry::JobEntry(int jobID, pid_t PID, const char *command, time_t startTime, Command *cmnd,
                   bool isStopped)
    : PID(PID),
      jobID(jobID),
      command(command),
      startTime(startTime),
      cmnd(cmnd),
      isStopped(isStopped)
{
    if (_isBackgroundComamnd(this->command))
    {
        isBackground = true;
    }
    else
    {
        isBackground = false;
    }
}

Command::Command(const char *cmd_line)
{
    // this->cmd_line = cmd_line;  // original cmd_line
    this->cmd_line = new char[strlen(cmd_line) + 1];
    strcpy(this->cmd_line, cmd_line);

    char **args = new char *[20];
    char *cmd_line_local = new char[strlen(cmd_line) + 1];
    strcpy(cmd_line_local, this->cmd_line);
    _trim(cmd_line_local);
    _removeBackgroundSign(cmd_line_local);

    this->num_args = _parseCommandLine(cmd_line_local, args);

    this->args = args;
}

// BuiltInCommand::BuiltInCommand(const char* cmd_line) : Command(cmd_line) {
//}

Command::~Command()
{
    for (int i = 0; args[i] != nullptr; i++)
    {
        free(args[i]);
        args[i] = nullptr;
    }
    delete[] args;
    args = nullptr;
}

ExternalCommand::ExternalCommand(const char *cmd_line) : Command(cmd_line) {}

////////////////////////////////////////////////////////////////////////
///                         Create Command                           ///
////////////////////////////////////////////////////////////////////////

/**
 * Creates and returns a pointer to Command class which matches the given
 * command line (cmd_line)
 * #CreateCommand
 */
Command *SmallShell::CreateCommand(const char *cmd_line)
{

    string cmd_s = _trim(string(cmd_line));
    if (cmd_s.length() == 0)
    {
        return nullptr;
    }
    string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
    if (firstWord[firstWord.size() - 1] == '&')
    {
        firstWord.pop_back();
    } // delete & connected to word

    char *new_line = new char[strlen(cmd_line) + 1];
    char **args = new char *[20];
    strcpy(new_line, cmd_line);
    _removeBackgroundSign(new_line);
    int num_args = _parseCommandLine(new_line, args);

    for (int i = 0; cmd_line[i] != '\0'; i++)
    {
        if (cmd_line[i] == '>' && cmd_line[i + 1] == '>')
        {
            return new RedirectionCommand(cmd_line, Append);
        }
        else if (cmd_line[i] == '>')
        {
            return new RedirectionCommand(cmd_line, Override);
        }
        else if (cmd_line[i] == '|' && cmd_line[i + 1] == '&')
        {
            return new PipeCommand(cmd_line, Fromstderr);
        }
        else if (cmd_line[i] == '|')
        {
            return new PipeCommand(cmd_line, Fromstdout);
        }
    }

    // Check for built-in commands
    if (firstWord.compare("chprompt") == 0)
    {
        return new ChangePromptCommand(cmd_line);
    }
    else if (firstWord.compare("showpid") == 0)
    {
        return new ShowPidCommand(cmd_line);
    }
    else if (firstWord.compare("pwd") == 0)
    {
        return new GetCurrDirCommand(cmd_line);
    }
    else if (firstWord.compare("cd") == 0)
    {
        return new ChangeDirCommand(cmd_line);
    }
    else if (firstWord.compare("jobs") == 0)
    {
        return new JobsCommand(cmd_line);
    }
    else if (firstWord.compare("fg") == 0)
    {
        return new ForegroundCommand(cmd_line);
    }
    else if (firstWord.compare("bg") == 0)
    {
        return new BackgroundCommand(cmd_line);
    }
    else if (firstWord.compare("quit") == 0)
    {
        return new QuitCommand(cmd_line);
    }
    else if (firstWord.compare("kill") == 0)
    {
        return new KillCommand(cmd_line);
    }

    // Check for special commands
    else if (firstWord.compare("setcore") == 0)
    {
        return new SetcoreCommand(cmd_line);
    }
    else if (firstWord.compare("getfiletype") == 0)
    {
        return new GetFileTypeCommand(cmd_line);
    }
    else if (firstWord.compare("chmod") == 0)
    {
        return new ChmodCommand(cmd_line);
    }
    else if (firstWord.compare("timeout") == 0)
    {
        return new TimeoutCommand(cmd_line);
    }
    else if (firstWord.compare("listdir") == 0)
    {
        return new ListDirCommand(cmd_line);
    }
    else if (firstWord.compare("getuser") == 0)
    {
        return new GetUserCommand(cmd_line);
    }

    // The Command is external
    return new ExternalCommand(cmd_line);

    return nullptr;
}
////////////////////////////////////////////////////////////////////////
///                        Built in Commands                         ///
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
///                         ChangePrompt Command                     ///
////////////////////////////////////////////////////////////////////////
void ChangePromptCommand::execute()
{
    SmallShell &smash = SmallShell::getInstance();

    if (!args[1])
    {
        smash.prompt = "smash";
        return;
    }
    smash.prompt = args[1];
}

////////////////////////////////////////////////////////////////////////
///                         ShowPid Command                          ///
////////////////////////////////////////////////////////////////////////
void ShowPidCommand::execute()
{

    SmallShell &smash = SmallShell::getInstance();
    std::cout << "smash pid is " << smash.shell_PID << std::endl;
}

////////////////////////////////////////////////////////////////////////
///                    GetCurrDir Command                            ///
////////////////////////////////////////////////////////////////////////
void GetCurrDirCommand::execute() { std::cout << getcwd(NULL, 0) << std::endl; }

////////////////////////////////////////////////////////////////////////
///                    ChangeDir Command                             ///
////////////////////////////////////////////////////////////////////////
void ChangeDirCommand::execute()
{
    SmallShell &smash = SmallShell::getInstance();
    std::string last_dir_local = getcwd(NULL, 0); // use only in case of change
    // Error Handling
    if (args[2] != nullptr)
    {
        std::cerr << "smash error: cd: too many arguments" << std::endl;
        return;
    }
    if (args[1] == nullptr)
    {
        return;
    }
    if (strcmp(this->args[1], "..") == 0)
    {
        // Find the last / and cut it
        std::string curr_dir = getcwd(NULL, 0);
        // curr_dir += "/";
        size_t index = curr_dir.find_last_of("/");

        if (index == std::string::npos)
        {
            // path is illegal, How did we get here?
            return;
        }
        curr_dir = curr_dir.substr(0, index);
        if (index == 0)
        {
            // case where curr is /home
            curr_dir = '/';
        }
        if (chdir(curr_dir.c_str()) != 0)
        {
            // chdir Failed
            perror("smash error: chdir failed");
            return;
        }
        else
        {
            smash.last_dir = last_dir_local;
        }
        smash.eventDirectoryHasChanged = true;
        return;
    }

    if (strcmp(this->args[1], "-") == 0)
    {
        if (smash.eventDirectoryHasChanged == false)
        {
            std::cerr << "smash error: cd: OLDPWD not set" << std::endl;
            return;
        }
        std::string set_to_this = smash.last_dir;
        if (chdir(set_to_this.c_str()) != 0)
        {
            // chdir Failed
            perror("smash error: chdir failed");
            return;
        }
        else
        {
            smash.last_dir = last_dir_local;
        }
        smash.eventDirectoryHasChanged = true;
        return;
    }

    // Normal path given.
    if (chdir(args[1]) != 0)
    {
        // chdir Failed
        perror("smash error: chdir failed");
        return;
    }
    else
    {
        smash.last_dir = last_dir_local;
    }
    smash.eventDirectoryHasChanged = true;
}

////////////////////////////////////////////////////////////////////////
///                             Jobs                                 ///
////////////////////////////////////////////////////////////////////////

void JobsList::removeFinishedJobs()
{
    if (!this->jobsList)
    {
        return;
    }
    if (this->jobsList->empty())
    {
        this->maxJobID = 1;
        return;
    }

    int stat_loc;
    for (std::list<JobEntry *>::iterator it = this->jobsList->begin(); it != this->jobsList->end(); it++)
    {
        int pid = (*it)->PID;
        if (waitpid((*it)->PID, &stat_loc, WNOHANG) > 0 || kill(pid, 0) == -1)
        {
            delete (*it)->cmnd;
            it = this->jobsList->erase(it);
        }
    }
    int new_max_id = 0;
    for (JobEntry *entry : *this->jobsList)
    {
        if (entry->jobID > new_max_id)
        {
            new_max_id = entry->jobID;
        }
    }
    this->maxJobID = new_max_id + 1;
}

void JobsList::addJob(Command *cmd, pid_t job_pid, bool isStopped)
{
    removeFinishedJobs();

    time_t time_of_start = time(nullptr);
    if (time_of_start == -1)
    {
        perror("smash error: time failed");
        return;
    }

    JobEntry *entry = new JobEntry(maxJobID, job_pid, cmd->cmd_line,
                                   time_of_start, cmd, isStopped);
    this->jobsList->push_back(entry);
    this->maxJobID++;
}

void JobsList::killAllJobs()
{
    SmallShell &smash = SmallShell::getInstance();
    smash.jobsList->removeFinishedJobs();
    for (JobEntry *entry : *jobsList)
    {
        if (kill(entry->PID, SIGKILL) == -1)
        {
            perror("smash error: kill failed");
        }
        else
        {
            delete entry;
        }
    }
    maxJobID = 1;
    for (std::list<JobEntry *>::iterator it = this->jobsList->begin(); it != this->jobsList->end(); it++)
    {
        delete (*it)->cmnd;
        it = this->jobsList->erase(it);
    }
    jobsList->clear();
}
void JobsList::printJobsList()
{
    removeFinishedJobs();
    for (JobEntry *entry : *jobsList)
    {
        std::cout << "[" << entry->jobID << "] " << entry->command;

        if (entry->isStopped)
        {
            std::cout << " (stopped)" << std::endl;
        }
        else
        {
            std::cout << std::endl;
        }
    }
}

JobEntry *JobsList::getJobById(int jobId)
{
    SmallShell &smash = SmallShell::getInstance();
    smash.jobsList->removeFinishedJobs();
    auto it = std::find_if(
        jobsList->begin(), jobsList->end(),
        [jobId](const JobEntry *entry)
        { return entry->jobID == jobId; });
    if (it == jobsList->end())
    {
        // Job doesn't exist
        return nullptr;
    }
    else
    {
        return *it;
    }
}

JobEntry *JobsList::getJobByPID(int PID)
{
    SmallShell &smash = SmallShell::getInstance();
    smash.jobsList->removeFinishedJobs();
    auto it = std::find_if(
        jobsList->begin(), jobsList->end(),
        [PID](const JobEntry *entry)
        { return entry->PID == PID; });
    if (it == jobsList->end())
    {
        // Job doesn't exist
        return nullptr;
    }
    else
    {
        return *it;
    }
}

void JobsList::removeJobById(int jobId)
{
    SmallShell &smash = SmallShell::getInstance();
    auto it = std::remove_if(
        jobsList->begin(), jobsList->end(),
        [jobId](const JobEntry *entry)
        { return entry->jobID == jobId; });
    if (it == jobsList->end())
    {
        return;
    }
    if (*it)
    {
        jobsList->erase(it);
        // delete *it; // todo without?
    }

    //    for (std::list<JobEntry*>::iterator it= this->jobsList->begin(); it != this->jobsList->end(); it++){
    //        if (jobId == (*it)->jobID) {
    //            delete (*it)->cmnd;
    //            it = this->jobsList->erase(it);
    //        }
    //    }
}
JobEntry *JobsList::getLastJob(int *lastJobId)
{
    SmallShell &smash = SmallShell::getInstance();
    smash.jobsList->removeFinishedJobs();
    if (this->jobsList->empty())
    {
        *lastJobId = -1; // Failure - No jobs
        return nullptr;
    }
    return jobsList->back();
}

JobEntry *JobsList::getLastStoppedJob(int *jobId)
{
    SmallShell &smash = SmallShell::getInstance();
    smash.jobsList->removeFinishedJobs();
    auto it = std::find_if(
        jobsList->rbegin(), jobsList->rend(),
        [jobId](const JobEntry *entry)
        { return entry->isStopped; });
    if (it == jobsList->rend())
    {
        return nullptr;
    }
    else
    {
        return *it;
    }
}

void JobsCommand::execute()
{
    SmallShell &smash = SmallShell::getInstance();
    smash.jobsList->removeFinishedJobs();
    smash.jobsList->printJobsList();
    smash.jobsList->removeFinishedJobs();
}

////////////////////////////////////////////////////////////////////////
///                       BackgroundCommand                          ///
////////////////////////////////////////////////////////////////////////

void ::BackgroundCommand::execute()
{
    SmallShell &smash = SmallShell::getInstance();
    smash.jobsList->removeFinishedJobs();

    /// no arguments
    if (num_args == 1)
    {
        int job_ID;

        if (smash.jobsList->getLastStoppedJob(&job_ID) == nullptr)
        {
            std::cerr << "smash error: bg: there is no stopped jobs to resume"
                      << endl;
            return;
        }

        JobEntry *my_job = smash.jobsList->getLastStoppedJob(&job_ID);

        cout << my_job->command << " " << my_job->PID << endl;

        if (kill(my_job->PID, SIGCONT) == -1)
        {
            perror("smash error: kill failed");
            return;
        }
        my_job->isStopped = false;
        my_job->isBackground = true;
        return;
    }
    /// with arguments

    int jobID;

    try
    {
        jobID = stoi((args[1]));
    }
    catch (...)
    {
        std::cerr << "smash error: bg: invalid arguments" << endl;
        return;
    }

    JobEntry *my_job = smash.jobsList->getJobById(jobID);

    if (my_job == nullptr)
    {
        cerr << "smash error: bg: job-id " << jobID << " does not exist"
             << endl;
        return;
    }

    if (my_job->isStopped == false)
    {
        cerr << "smash error: bg: job-id " << jobID
             << " is already running in the background" << endl;
        return;
    }

    if (num_args >= 3)
    {
        std::cerr << "smash error: bg: invalid arguments" << endl;
        return;
    }

    cout << my_job->command << " " << my_job->PID << endl;

    if (kill(my_job->PID, SIGCONT) == -1)
    {
        perror("smash error: kill failed");
        return;
    }

    my_job->isBackground = true;
    my_job->isStopped = false;
}

////////////////////////////////////////////////////////////////////////
///                             QuitCommand                          ///
////////////////////////////////////////////////////////////////////////
void QuitCommand::execute()
{

    SmallShell &smash = SmallShell::getInstance();
    smash.jobsList->removeFinishedJobs();
    if (num_args == 1)
    {
        exit(0);
    }

    if (strcmp(args[1], "kill") != 0)
    { // #cont
        exit(0);
    }
    // print before killing

    int counter = 0;
    for (JobEntry *entry : *smash.jobsList->jobsList)
    {
        counter++;
    }
    std::cout << "smash: sending SIGKILL signal to " << counter << " jobs:" << std::endl;
    for (JobEntry *entry : *smash.jobsList->jobsList)
    {
        std::cout << entry->PID << entry->command << std::endl;
    }
    // std::cout << std::endl;
    smash.jobsList->killAllJobs();
    exit(0);
}

////////////////////////////////////////////////////////////////////////
///                       ForegroundCommand                          ///
////////////////////////////////////////////////////////////////////////
void ForegroundCommand::execute()
{

    SmallShell &smash = SmallShell::getInstance();
    smash.jobsList->removeFinishedJobs();

    if (num_args == 1)
    {
        int job_ID;

        if (smash.jobsList->getLastJob(&job_ID) == nullptr)
        {
            std::cerr << "smash error: fg: jobs list is empty"
                      << endl;
            return;
        }

        JobEntry *my_job = smash.jobsList->getLastJob(&job_ID);

        // Cont. job in case it has been stopped
        if (my_job->isStopped)
        {
            if (kill(my_job->PID, SIGCONT) == -1)
            {
                perror("smash error: kill failed");
                return;
            }
            my_job->isStopped = false;
        }
        cout << my_job->command << " " << my_job->PID << endl;

        smash.UpdateForeground(my_job->cmnd, my_job->PID);
        int temp_pid = my_job->PID;

        waitpid(temp_pid, nullptr, WUNTRACED);

        smash.UpdateForeground(nullptr, -1);
        smash.jobsList->removeFinishedJobs();
        return;
    }

    int jobID;

    try
    {
        jobID = stoi((args[1]));
    }
    catch (...)
    {
        std::cerr << "smash error: fg: invalid arguments" << endl;
        return;
    }

    JobEntry *my_job = smash.jobsList->getJobById(jobID);

    if (my_job == nullptr)
    {
        cerr << "smash error: fg: job-id " << jobID << " does not exist"
             << endl;
        return;
    }

    if (num_args >= 3)
    {
        std::cerr << "smash error: fg: invalid arguments" << endl;
        return;
    }

    // Cont. job in case it has been stopped
    if (my_job->isStopped)
    {
        if (kill(my_job->PID, SIGCONT) == -1)
        {
            perror("smash error: kill failed");
            return;
        }
        my_job->isStopped = false;
    }

    cout << my_job->command << " " << my_job->PID << endl;

    smash.UpdateForeground(my_job->cmnd, my_job->PID);
    int temp_pid = my_job->PID;

    waitpid(temp_pid, nullptr, WUNTRACED);
    smash.UpdateForeground(nullptr, -1);
    smash.jobsList->removeFinishedJobs();
}

void TimeoutCommand::execute() {}

bool TextIsNumber(const char *text);

////////////////////////////////////////////////////////////////////////
///                           KillCommand                            ///
////////////////////////////////////////////////////////////////////////

void KillCommand::execute()
{ /// remember to ask if checking the validity of
  /// jobID is always necessary!! (not only here
  /// but everywhere)

    SmallShell &smash = SmallShell::getInstance();
    smash.jobsList->removeFinishedJobs();

    std::string jobNumStr;
    int jobID;
    try
    {
        jobNumStr = std::string(args[2]);
        jobID = stoi(jobNumStr);
    }
    catch (...)
    {
        std::cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }

    if (!TextIsNumber(jobNumStr.c_str()))
    {
        std::cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }

    JobEntry *my_job = smash.jobsList->getJobById(jobID);
    if (my_job == nullptr)
    {
        cerr << "smash error: kill: job-id " << jobID << " does not exist"
             << endl;
        return;
    }

    std::string numStr = std::string(args[1]).substr(1);

    int sigNum;
    try
    {
        sigNum = stoi(numStr);
    }
    catch (...)
    {
        std::cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }

    if (num_args !=
        3)
    { // make sure that this is enough, rather than checking if #<3
        std::cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }

    char *sig_num_string = new char[strlen(args[1]) + 1];
    strcpy(sig_num_string, args[1]);
    if (sig_num_string[0] != '-')
    {
        std::cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }

    sig_num_string = sig_num_string + 1;
    if (!TextIsNumber(sig_num_string))
    {
        std::cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }

    pid_t pid = my_job->PID;
    switch (sigNum)
    {
    case SIGSTOP:
        my_job->isStopped = true;
        break;
    case SIGCONT:
        my_job->isStopped = false;
        break;
    }
    if (kill(my_job->PID, sigNum) == -1)
    {
        perror("smash error: kill failed");
        return;
    }
    smash.jobsList->removeFinishedJobs();
    if (sigNum == SIGKILL)
    {
        smash.jobsList->removeJobById(my_job->jobID);
    }
    std::cout << "signal number " << sigNum << " was sent to pid " << pid << std::endl;
}

////////////////////////////////////////////////////////////////////////
///                         #External Commands                       ///
////////////////////////////////////////////////////////////////////////

void SmallShell::UpdateForeground(Command *command, pid_t pid)
{
    SmallShell &smash = SmallShell::getInstance();
    smash.foreground_command = command;
    smash.foreground_pid = pid;
}

void ExternalCommand::execute()
{
    SmallShell &smash = SmallShell::getInstance();
    smash.jobsList->removeFinishedJobs();
    bool isBackground = _isBackgroundComamnd(cmd_line);
    char *cmd_line_local = new char[strlen(cmd_line) + 1];
    strcpy(cmd_line_local, this->cmd_line);
    _removeBackgroundSign(cmd_line_local);
    char **args = new char *[20];
    int arg_count = _parseCommandLine(cmd_line_local, args);
    bool isComplex = false;
    // Check type

    for (int i = 0; i < arg_count; i++)
    {
        if (std::strchr(args[i], '*') != nullptr ||
            std::strchr(args[i], '?') != nullptr)
        {
            isComplex = true;
        }
    }

    pid_t pid = fork();
    if (pid == -1)
    {
        perror("smash error: fork failed");
        return;
    }
    if (pid == 0)
    { // Child Process
        setpgrp();
        if (isComplex)
        {
            if (execl("/bin/bash", "/bin/bash", "-c", cmd_line, NULL) == -1)
            {
                perror("smash error: execl failed"); // TODO execl or exec
                exit(EXIT_FAILURE);
            }
        }
        else
        { // Simple
            if (execvp(args[0], args) == -1)
            {
                perror("smash error: execvp failed"); // TODO same
                exit(EXIT_FAILURE);
            }
        }
        exit(EXIT_SUCCESS);
    }
    else
    { // Parent process

        if (isBackground)
        {

            smash.jobsList->addJob(this, pid);
        }
        else
        { // foreground
            smash.UpdateForeground(this, pid);
            waitpid(pid, nullptr, WUNTRACED);
            smash.UpdateForeground(nullptr, -1);
            smash.jobsList->removeFinishedJobs();
        }
    }
}

////////////////////////////////////////////////////////////////////////
///                         #RedirectionCommand                        ///
////////////////////////////////////////////////////////////////////////

void RedirectCommandIntoTwoParts(std::string *string_cmd_line,
                                 RedirectType type, std::string *command,
                                 std::string *output_file)
{
    if (type == Append)
    { // >>

        size_t redirection_position = string_cmd_line->find(">>");
        *command = _trim(string_cmd_line->substr(0, redirection_position));
        *output_file = _trim(string_cmd_line->substr(redirection_position + 2));
    }
    else if (type == Override)
    { // >

        size_t redirection_position = string_cmd_line->find(">");
        *command = _trim(string_cmd_line->substr(0, redirection_position));
        *output_file = _trim(string_cmd_line->substr(redirection_position + 1));
    }

    // Ignore background sign in both CMD and File
    _removeBackgroundSign((char *)(command->c_str()));
    _removeBackgroundSign((char *)(output_file->c_str()));

    // Trim again for good measure
    *command = _trim(*command);
    *output_file = _trim(*output_file);
}

void RedirectionCommand::execute()
{
    SmallShell &smash = SmallShell::getInstance();

    std::string string_cmd_line(this->cmd_line);
    std::string command, output_file;

    // Split command into two usable parts
    RedirectCommandIntoTwoParts(&string_cmd_line, this->type, &command,
                                &output_file);

    // We now want to use the Open system call to open the requested file
    // descriptor, Which should open in the lowest available integer

    int requested_file_desc =
        open(output_file.c_str(),
             (this->type == Append) ? O_CREAT | O_WRONLY | O_APPEND
                                    : O_CREAT | O_WRONLY | O_TRUNC,
             0655);

    if (requested_file_desc == -1)
    {
        perror("smash error: open failed");
        return;
    }

    // We now want to close the normal std_out and duplicate it into the
    // variable new_std_out
    int new_std_out = dup(1); // 1 is std_out
    if (new_std_out == -1)
    {
        perror("smash error: dup failed");
        return;
    }
    int res = close(1); // close original std_out
    if (res == -1)
    {
        perror("smash error: close failed");
        return;
    }

    if (dup2(requested_file_desc, 1) == -1)
    {
        perror("smash error: dup2 failed");
        return;
    }

    // We now want to execute the command that is found in the first portion
    smash.executeCommand(command.c_str());

    // Close the file we opened
    if (close(requested_file_desc) == -1)
    {
        perror("smash error: close failed");
        return;
    }

    // try
    if (close(1) == -1)
    {
        perror("smash error: close failed");
        return;
    }

    // return stdout to its oriignal place
    if (dup2(new_std_out, 1) == -1)
    {
        perror("smash error: dup2 failed");
        return;
    }

    //  unneccesary copy of std_out
    if (close(new_std_out) == -1)
    {
        perror("smash error: close failed");
        return;
    }
}

////////////////////////////////////////////////////////////////////////
///                         #pipeCommand                             ///
////////////////////////////////////////////////////////////////////////

void PipeCommandIntoTwoParts(std::string *string_cmd_line, PipeType type,
                             std::string *command, std::string *output_file)
{
    if (type == Fromstderr)
    { // |&

        size_t pipe_position = string_cmd_line->find("|&");
        *command = _trim(string_cmd_line->substr(0, pipe_position));
        *output_file = _trim(string_cmd_line->substr(pipe_position + 2));
    }
    else if (type == Fromstdout)
    { // |

        size_t pipe_position = string_cmd_line->find("|");
        *command = _trim(string_cmd_line->substr(0, pipe_position));
        *output_file = _trim(string_cmd_line->substr(pipe_position + 1));
    }

    // Ignore background sign in both CMD and File
    _removeBackgroundSign((char *)(command->c_str()));
    _removeBackgroundSign((char *)(output_file->c_str()));

    // Trim again for good measure
    *command = _trim(*command);
    *output_file = _trim(*output_file);
}

void PipeCommand::execute()
{
    int pipe_read = 0;
    int pipe_write = 1;
    int STD_OUT_INDEX = 1;
    int STD_ERR_INDEX = 2;
    int pipe_file_desc[2];

    SmallShell &smash = SmallShell::getInstance();

    std::string string_cmd_line(this->cmd_line);
    std::string command1, command2;

    // Split command into two usable parts
    PipeCommandIntoTwoParts(&string_cmd_line, this->type, &command1, &command2);

    // Create the two commands
    Command *Command1 = smash.CreateCommand(command1.c_str());
    Command *Command2 = smash.CreateCommand(command2.c_str());

    // Create the pipe that will be used
    if (pipe(pipe_file_desc) == -1)
    {
        ::perror("smash error: pipe failed");
        return;
    }

    // We will now fork twice, once for each CommandI, and execute the command
    // on each process and for each one redirect the requested channel to the
    // appropriate end of the pipe

    pid_t command_1_pid = fork();

    if (command_1_pid == -1)
    {
        perror("smash error: fork failed");
        return;
    }

    if (command_1_pid == 0)
    {
        if (setpgrp())
        {
            perror("smash error: setpgrp failed");
            return;
        }
        // Command 1 proccess
        // Close Read channel of pipe
        //        if(close(pipe_file_desc[pipe_read]) == -1){
        //            perror("smash error: close failed");
        //            return;
        //        }
        // Redirect Write channel of pipe to STDOUT or STDERR
        if (type == Fromstdout)
        {
            if (dup2(pipe_file_desc[pipe_write], STD_OUT_INDEX) == -1)
            {
                perror("smash error: dup2 failed");
                return;
            }
        }
        else if (type == Fromstderr)
        {
            if (dup2(pipe_file_desc[pipe_write], STD_ERR_INDEX) == -1)
            {
                perror("smash error: dup2 failed");
                return;
            }
        }

        // Close unneccesary channels
        if (close(pipe_file_desc[pipe_read]) == -1)
        {
            perror("smash error: close failed");
            return;
        }
        if (close(pipe_file_desc[pipe_write]) == -1)
        {
            perror("smash error: close failed");
            return;
        }

        // Then execute the appropriate command 1
        smash.executeCommand(command1.c_str());
        // if we are still here, failure

        exit(EXIT_FAILURE);
    }

    pid_t command_2_pid = fork();

    if (command_2_pid == -1)
    {
        perror("smash error: fork failed");
        return;
    }

    int STD_IN_INDEX = 0;

    if (command_2_pid == 0)
    {
        if (setpgrp())
        {
            perror("smash error: setpgrp failed");
            return;
        }

        // Redirect Read channel of pipe to stdin of proccess 2
        if (dup2(pipe_file_desc[pipe_read], STD_IN_INDEX) == -1)
        {
            perror("smash error: dup2 failed");
            return;
        }

        // Close unneccesary channels
        if (close(pipe_file_desc[pipe_read]) == -1)
        {
            perror("smash error: close failed");
            return;
        }
        if (close(pipe_file_desc[pipe_write]) == -1)
        {
            perror("smash error: close failed");
            return;
        }

        // Then execute the appropriate command 1
        smash.executeCommand(command2.c_str());
        // if we are still here, failure

        exit(EXIT_FAILURE);
    }

    // Now for the parent proccess, we want to close both channels

    // Close unneccesary channels
    if (close(pipe_file_desc[pipe_read]) == -1)
    {
        perror("smash error: close failed");
        return;
    }
    if (close(pipe_file_desc[pipe_write]) == -1)
    {
        perror("smash error: close failed");
        return;
    }

    if (waitpid(command_2_pid, nullptr, WUNTRACED) == -1 ||
        waitpid(command_1_pid, nullptr, WUNTRACED) == -1)
    {
        perror("smash error: waitpid failed");
        return;
    }
}

////////////////////////////////////////////////////////////////////////
///                              #SetCore                             ///
////////////////////////////////////////////////////////////////////////

// Simple Auxiliary function that checks if a given char* is a number
bool TextIsNumber(const char *text)
{
    int pos = 0;
    char NULL_TERM = '\0';
    while (text[pos] != NULL_TERM)
    {
        if (!isdigit(text[pos]))
        {
            return false;
        }
        pos++;
    }
    return true;
}

void SetcoreCommand::execute()
{
    SmallShell &smash = SmallShell::getInstance();
    char *job_id = args[1];
    char *requested_cpu = args[2];
    int online_cpus = sysconf(_SC_NPROCESSORS_ONLN);

    if (TextIsNumber(job_id))
    {
        if (smash.jobsList->getJobById(atoi(job_id)) == nullptr)
        {
            std::cerr << "smash error: setcore: job-id " << job_id
                      << " does not exist" << std::endl;
            return;
        }
    }
    else
    {
        std::cerr << "smash error: setcore: invalid arguments" << endl;
        return;
    }

    if (TextIsNumber(requested_cpu))
    {
        int requested_cpu_num = atoi(requested_cpu);
        if (requested_cpu_num < 0 || requested_cpu_num >= online_cpus)
        {
            std::cerr << "smash error: setcore: invalid core number"
                      << std::endl;
            return;
        }
    }
    else
    {
        std::cerr << "smash error: setcore: invalid arguments" << endl;
        return;
    }

    if (!(TextIsNumber(requested_cpu)) || !(TextIsNumber(job_id)) ||
        (args[3] != nullptr))
    {
        std::cerr << "smash error: setcore: invalid arguments" << std::endl;
        return;
    }

    int requested_cpu_num = atoi(requested_cpu);
    JobEntry *requested_job = smash.jobsList->getJobById(atoi(job_id));

    cpu_set_t cpu;
    CPU_ZERO(&cpu);
    CPU_SET(requested_cpu_num, &cpu);

    if (sched_setaffinity(requested_job->PID, sizeof(cpu), &cpu) == -1)
    {
        // TODO review errors, are they what is expected?
        if (errno == ESRCH)
        {
            std::cerr << "smash error: setcore: job-id " << job_id
                      << " does not exist" << std::endl;
            return;
        }
        else if (errno == EINVAL)
        {
            std::cerr << "smash error: setcore: invalid core number"
                      << std::endl;
            return;
        }
        perror("smash error: sched_setaffinity failed");
        return;
    }
}

////////////////////////////////////////////////////////////////////////
///                            #GetFileTypeCommand                   ///
////////////////////////////////////////////////////////////////////////

void GetFileTypeCommand::execute()
{
    struct stat file_info;
    char *file_path = args[1];

    if (this->args[2] != nullptr)
    {
        cerr << "smash error: gettype: invalid arguments" << endl;
        return;
    }

    if (this->args[1] == nullptr)
    {
        cerr << "smash error: gettype: invalid arguments" << endl;
        return;
    }

    if (stat(file_path, &file_info) == -1)
    {

        perror("smash error: stat failed");
        return;
    }

    std::cout << file_path << "'s type is \"";

    if (S_ISREG(file_info.st_mode))
    {
        std::cout << "regular file";
    }
    else if (S_ISDIR(file_info.st_mode))
    {
        std::cout << "directory";
    }
    else if (S_ISCHR(file_info.st_mode))
    {
        std::cout << "character device";
    }
    else if (S_ISBLK(file_info.st_mode))
    {
        std::cout << "block device";
    }
    else if (S_ISFIFO(file_info.st_mode))
    {
        std::cout << "FIFO";
    }
    else if (S_ISLNK(file_info.st_mode))
    {
        std::cout << "symbolic link";
    }
    else if (S_ISSOCK(file_info.st_mode))
    {
        std::cout << "socket";
    }

    std::cout << "\" and takes up " << file_info.st_size << " bytes" << endl;
}

////////////////////////////////////////////////////////////////////////
///                            #ChmodCommand                        ///
////////////////////////////////////////////////////////////////////////
void ChmodCommand::execute()
{
    // SmallShell& smash = SmallShell::getInstance();
    if (args[3] != nullptr)
    {
        cerr << "smash error: chmod: invalid arguments" << endl;
        return;
    }
    if (!TextIsNumber(args[1]))
    {
        cerr << "smash error: chmod: invalid arguments" << endl;
        return;
    }

    int mode = std::stoi(args[1], nullptr, 8);

    char *file_path = args[2];

    if (chmod(file_path, mode) == -1)
    {
        perror("smash error: chmod failed");
        return;
    }
}