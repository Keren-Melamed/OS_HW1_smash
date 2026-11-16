// Ver: 20-10-2026
#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <list>
#include <map>
#include<regex>
#include <unordered_map>
#include <pthread.h>

#define COMMAND_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

class Command {
protected:
    const char* cmdLine;
    char** arguments;
    int numOfArguments;
    const std::list<std::string>* reserved_commands;

    // TODO: Add your data members
public:

    Command(const char *cmd_line);

    virtual ~Command();

    virtual void execute() = 0;

    virtual pid_t getSmashPid() const;

    std::string printableCommand();

    int isNumber(std::string arg); //returns the number if arg is a number, -1 otherwise

    char** removeAmpersandFromArguments(); //removes "&"" from last argument (or as the last argument) if exists and returns the modified arguments array. Replacing removeAmpersand
    char** getArguments();
    //virtual void prepare();
    //virtual void cleanup();
    // TODO: Add your extra methods if needed
    virtual const char* getCmdLine();
};

class BuiltInCommand : public Command {
public:
    BuiltInCommand(const char *cmd_line);

    virtual ~BuiltInCommand() {
    }
};

class JobsList {
public:
    class JobEntry {
        public:
            int jobID;
            Command* job_command;
            pid_t pid;
            bool isStopped;

            JobEntry(int jobID, Command *job_command, pid_t pid);
            virtual ~JobEntry(){}
            void printJobEntry();
    };

    std::list<JobEntry>* jobs_list;
    int numOfJobs;

public:

    JobsList();

    ~JobsList(){}

    void addJob(Command *cmd, pid_t processID, bool isStopped = false);//done

    void printJobsList();//done

    void killAllJobs();

    void removeFinishedJobs();

    JobEntry *getJobById(int jobId); //done

    void removeJobById(int jobId); //done

    JobEntry *getLastJob(); //done

    JobEntry *getLastStoppedJob(int *jobId);

    // TODO: Add extra methods or modify existing ones as needed
};

class ExternalCommand : public Command {
public:
    ExternalCommand(const char *cmd_line);

    virtual ~ExternalCommand() {
    }

    void execute() override;
};


class RedirectionCommand : public Command {
    // TODO: Add your data members
public:
    explicit RedirectionCommand(const char *cmd_line);

    virtual ~RedirectionCommand() {
    }

    void execute() override;
};

class PipeCommand : public Command {
    // TODO: Add your data members
public:
    PipeCommand(const char *cmd_line);

    virtual ~PipeCommand() {
    }

    void execute() override;
};

class DiskUsageCommand : public Command {
public:
    DiskUsageCommand(const char *cmd_line);

    virtual ~DiskUsageCommand() {
    }

    void execute() override;
};

class WhoAmICommand : public Command {
public:
    WhoAmICommand(const char *cmd_line);

    virtual ~WhoAmICommand() {
    }

    void execute() override;
};

class TimeoutCommand : public Command {
    // TODO: Add your data members **BONUS: 10 Points**
public:
    TimeoutCommand(const char *cmd_line);

    virtual ~TimeoutCommand() {
    }

    void execute() override;
};

class ChangePromptCommand : public BuiltInCommand{
    ChangePromptCommand(const char* cmd_line);

    virtual ~ChangePromptCommand(){

    }
    void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {
    
    char** previousPWD;
    // TODO: Add your data members public:
    ChangeDirCommand(const char *cmd_line, char **plastPwd);

    virtual ~ChangeDirCommand() {
    }

    void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
public:
    GetCurrDirCommand(const char *cmd_line);

    virtual ~GetCurrDirCommand() {
    }

    void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
public:
    ShowPidCommand(const char *cmd_line);

    virtual ~ShowPidCommand() {
    }

    void execute() override;
};

class QuitCommand : public BuiltInCommand {
    // TODO: Add your data members public:
    QuitCommand(const char *cmd_line, JobsList *jobs);

    virtual ~QuitCommand() {
    }

    void execute() override;
};

class JobsList {
public:
    class JobEntry {
        public:
            int jobID;
            Command* job_command;
            pid_t pid;
            bool isStopped;

            JobEntry(int jobID, Command *job_command, pid_t pid);
            virtual ~JobEntry(){}
            void printJobEntry();
    };

    std::list<JobEntry>* jobs_list;
    int numOfJobs;

public:

    JobsList();

    ~JobsList(){}

    void addJob(Command *cmd, pid_t processID, bool isStopped = false);//done

    void printJobsList();//done

    void killAllJobs();

    void removeFinishedJobs();

    JobEntry *getJobById(int jobId); //done

    void removeJobById(int jobId); //done

    JobEntry *getLastJob(); //done

    JobEntry *getLastStoppedJob(int *jobId);

    // TODO: Add extra methods or modify existing ones as needed
};

class JobsCommand : public BuiltInCommand {
    // TODO: Add your data members
public:
    JobsCommand(const char *cmd_line, JobsList *jobs);

    virtual ~JobsCommand() {
    }

    void execute() override;
};

class KillCommand : public BuiltInCommand {

public:

    KillCommand(const char *cmd_line);

    virtual ~KillCommand() {
    }

    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
    // TODO: Add your data members
public:

    ForegroundCommand(const char *cmd_line);

    virtual ~ForegroundCommand(){}

    void execute() override;
};

class AliasCommand : public BuiltInCommand {
public:
    std::string aliasName;
    std::string aliasCmd;
    AliasCommand(const char *cmd_line);

    virtual ~AliasCommand(){}

    void execute() override;
    void printExistingAliases();
    bool nameReservedOrExists();
    bool nameArgumentLegal();
    
};

class UnAliasCommand : public BuiltInCommand {
public:
    UnAliasCommand(const char *cmd_line);

    virtual ~UnAliasCommand(){}
    
    void execute() override;
};

class UnSetEnvCommand : public BuiltInCommand {
public:
    UnSetEnvCommand(const char *cmd_line);

    virtual ~UnSetEnvCommand() {
    }

    void execute() override;
};

class GetFileTypeCommand : public BuiltInCommand {
public:
    GetFileTypeCommand(const char *cmd_line);

    virtual ~GetFileTypeCommand() {
    }

    void execute() override;
};

class SmallShell {
private:
    // TODO: Add your data members
    std::string prompt = "smash";
    //JobsList jobs; - theres a linked list of jobs under public
    std::unordered_map<std::string, Command*> aliases;

    char* OLDPWD = nullptr;
    char* promptMessage;
    int foregroundPID;

    SmallShell();

public:
    std::list<std::pair<std::string, std::string>>* existing_aliases; //aliases are the keys
    JobsList* jobs; //HERE

    Command *CreateCommand(const char *cmd_line);

    SmallShell(SmallShell const &) = delete; // disable copy ctor
    void operator=(SmallShell const &) = delete; // disable = operator
    static SmallShell &getInstance() // make SmallShell singleton
    {
        static SmallShell instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }

    ~SmallShell();

    void executeCommand(const char *cmd_line);
    char** oldPwdPointer();
    std::list<std::pair<std::string, std::string>>* getExistingAliases();
    char* getPromptMessage() const;

    void setCurrentForegroundPID(int pid);
    int getCurrentForegroundPID();
    JobsList* getJobsListPointer();
    bool isAlias(std::string name);
    void removeAlias(const std::string& key_to_remove);
    std::string getAliasCmdByName(std::string name);
    


    // TODO: add extra methods as needed
    std::string getPrompt();

    void setPrompt(std::string newPrompt);

    void printAliases();

    void addAlias(std::string alias, Command* cmd);
};
#endif //SMASH_COMMAND_H_
