#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include <regex>
#include "Commands.h"

//I added this to use PATH_MAX
#include <limits.h>


using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";

const int MAX_ARGS_NUM = 20;

extern char** environ;

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

string _ltrim(const std::string &s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string &s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string &s) {
    return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char *cmd_line, char **args) {
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for (std::string s; iss >> s;) {
        args[i] = (char *) malloc(s.length() + 1);
        memset(args[i], 0, s.length() + 1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;
    FUNC_EXIT()
}

bool _isBackgroundComamnd(const char *cmd_line) {
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char *cmd_line) {
    const string str(cmd_line);
    // find last character other than spaces
    unsigned int idx = str.find_last_not_of(WHITESPACE);
    // if all characters are spaces then return
    if (idx == string::npos) {
        return;
    }
    // if the command line does not end with & then return
    if (cmd_line[idx] != '&') {
        return;
    }
    // replace the & (background sign) with space and then remove all tailing spaces.
    cmd_line[idx] = ' ';
    // truncate the command line string up to the last non-space character
    cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

// TODO: Add your implementation for classes in Commands.h 

Command::Command(const char* cmd_line){
    this->cmdLine = cmd_line;
    this->arguments = (char**)malloc(MAX_ARGS_NUM * sizeof(char*));
    this->numOfArguments = _parseCommandLine(cmd_line, this->arguments);
    // remove '&' if it exists. still stays on cmdLine.
    _removeBackgroundSign(this->arguments[this->numOfArguments - 1]);
}

Command::~Command(){
    for (int i = 0; i < this->numOfArguments; i++){
        free(this->arguments[i]);
    }
    free(this->arguments);
}

pid_t Command::getSmashPid() const {
    //getpid is always successful
    return getpid();
}

const char* Command::getCmdLine(){
    return this->cmdLine;
}

ExternalCommand::ExternalCommand(const char* cmd_line) : Command(cmd_line){

}

void ExternalCommand::execute(){
    int pos_wildcard1 = string(this->cmdLine).find('*');
    int pos_wildcard2 = string(this->cmdLine).find('?');
    bool isBackground = _isBackgroundComamnd(this->cmdLine);
    pid_t pid = fork();
    if (pid == 0){
        if (pos_wildcard1 == string::npos && pos_wildcard2 == string::npos){
            execvp(this->arguments[0], this->arguments);
        }
        else{
            // TODO: handle & in cmdLine here
            execl("/bin/bash", "bash", "-c", this->cmdLine, (char*)nullptr);
        }
    }
    else{
        if (!isBackground){
            waitpid(pid, NULL, 0);
        }
        else{
            SmallShell::getInstance().getJobsListPointer()->addJob(this, pid);
        }
    }
}

void ChangePromptCommand::execute(){
    std::string newPrompt;
    if (this->numOfArguments == 1){
        newPrompt = "smash";
    }
    else{
        newPrompt = std::string(this->arguments[1]);
    }
    SmallShell::getInstance().setPrompt(newPrompt);
}

ShowPidCommand::ShowPidCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void ShowPidCommand::execute() {
    //getpid is always successful
    std::cout << "smash pid is " << getSmashPid() << std::endl;
}

void GetCurrDirCommand::execute(){
    char buffer[PATH_MAX];
    getcwd(buffer, sizeof(buffer));
    std::cout << buffer << std::endl;
}

void ChangeDirCommand::execute() {
    //No arguments - no effect
    if (this->numOfArguments == 1) {
        return;
    } else if (this->numOfArguments == 2) {
        //Not NULL for sure
        char* arg = this->arguments[1];
        //Backup the current directory
        char currentPath[PATH_MAX];
        //getcwd returns NULL if fails.
        if (!(getcwd(currentPath, PATH_MAX))) {
            perror("smash error: getcwd failed");
            return;
        }
        char* currentPathPointer = strdup(currentPath);
        if (*arg == '-') {
            //The previous directory exists (not null)
            if (*this->previousPWD) {
                //Change directory to the previous directory
                //If chdir fails, -1 is returned.
                if (chdir(*this->previousPWD) == -1) {
                    perror("smash error: chdir failed");
                    return;
                }
            } else {
                //no previous PWD! ERROR!
                std::cerr << "smash error: cd: OLDPWD not set" << std::endl;
            }
        } else {
            //Change directory to the new given one
            //If chdir fails, -1 is returned.
            if (chdir(arg) == -1) {
                perror("smash error: chdir failed");
                return;
            }
        }
        //Set the previous directory as the directory we were in before executing
        delete *this->previousPWD;
        *this->previousPWD = currentPathPointer;
    } else {
        //too many arguments (more than 1)
        std::cerr << "smash error: cd: too many arguments" << std::endl;
        return;
    }
}

void JobsCommand::execute(){
    SmallShell::getInstance().getJobsListPointer()->printJobsList();
}

void QuitCommand::execute(){
    if (this->numOfArguments > 1){
        for(int i = 1; i < this->numOfArguments; i++){
            if (string(this->arguments[i]) == "kill"){
                this->jobs->killAllJobs();
            }
        }
    }
    SmallShell::getInstance().setIsActive(false);
}

void AliasCommand::execute(){
    if (this->numOfArguments == 1){
        SmallShell::getInstance().printAliases();
    }
    else{
        std::regex arg_pattern("^alias [a-zA-Z0-9_]+='[^']*'$");
        if (!std::regex_match(this->cmdLine, arg_pattern)){ //check is in correct format
            std::cout << "smash error: alias: invalid alias format";
            return;
        }
        int pos = std::string(this->arguments[1]).find('=');
        std::string alias = std::string(this->arguments[1]).substr(0, pos);
        if(dynamic_cast<ExternalCommand*>(SmallShell::getInstance().CreateCommand(alias.c_str())) == nullptr){ //check isn't builtin
            std::cout << "smash error: alias: " << alias << " already exists or is a reserved command ";
            return;
        } 
        else{
            std::string cmdLine = std::string(this->arguments[1]).substr(pos + 1);
            Command* cmd = SmallShell::getInstance().CreateCommand(cmdLine.c_str());
            SmallShell::getInstance().addAlias(alias, cmd);
        }
    }
}

void UnSetEnvCommand::execute(){
    if (this->numOfArguments == 1){
        std::cout << "smash error: unsetenv: not enough arguments" << std::endl;
        return;
    }
    std::string smashPid = std::to_string(this->getSmashPid());
    std::string envPath = "/proc/" + smashPid + "/environ";
    FILE *fptr = fopen(envPath.c_str(), "r");
    char buffer[256];
    //Env vars are null seperated, so this reads the whole file
    fgets(buffer, sizeof(buffer), fptr);
    fclose(fptr);
    std::vector<std::string> envVars;
    std::stringstream ss(buffer);
    std::string envVar;
    //Get environment variables
    while(std::getline(ss, envVar, '\0')){
        int pos = envVar.find('=');
        envVars.push_back(envVar.substr(0, pos));
    }
    for(int i = 1; i < this->numOfArguments; i++){
        if(std::find(envVars.begin(), envVars.end(), this->arguments[i]) == envVars.end()){
            std::cout << "smash error: unsetenv:" << this->arguments[i] << "does not exist" << std::endl;
            return;
        }
        int varLen = strlen(this->arguments[i]);
        for(int j = 0; environ[j] != nullptr; j++){
            if(strncmp(environ[j], this->arguments[i], varLen) == 0 && environ[j][varLen] == '='){
                for(int k = j; environ[k] != nullptr; k++){
                    environ[k] = environ[k+1];
                }
                break;
            }
        }
    }
}

AliasCommand::AliasCommand(const char *cmd_line) : BuiltInCommand(cmd_line){
}

bool AliasCommand::nameReservedOrExists(){
}


UnAliasCommand::UnAliasCommand(const char *cmd_line) : BuiltInCommand(cmd_line){}

void UnAliasCommand::execute(){

    if(numOfArguments == 1){
        std::cerr << "smash error: unalias: not enough arguments" << std::endl;
        return;
    }
    list<pair<string, string>>*  aliases = SmallShell::getInstance().getExistingAliases();

    if(aliases == nullptr || aliases->empty()){
        return;
    }

    for(int i = 1; arguments[i] != nullptr; i++){

        string arg(arguments[i]);

        if(SmallShell::getInstance().isAlias(arg)){
            SmallShell::getInstance().removeAlias(arg);
        }
        else{
            std::cerr << "smash error: unalias: " << arg << " alias does not exist" << std::endl;
            return;
        }
    }
    return;
}

JobsList::JobEntry::JobEntry(int jobID, Command *job_command, pid_t pid){
    this->jobID = jobID;
    this->job_command = job_command;
    this->pid = pid;
}

JobsList::JobEntry::~JobEntry(){
    delete this->job_command;
}

void JobsList::JobEntry::printJobEntry(){
    if(this->job_command == nullptr){
        return;
    }

    string cmd = this->job_command->printableCommand();

    std::cout<< "[" << this->jobID <<"]" << cmd << endl;
}

JobsList::JobsList(){
    this->jobs_list = new std::list<JobEntry>();
    numOfJobs = 0;
}

void JobsList::addJob(Command *cmd, pid_t processID, bool isStopped){
    if(this->jobs_list == nullptr){
        return;
    }

    this->removeFinishedJobs();
    this->numOfJobs = this->jobs_list->size();

    if(this->jobs_list->empty()){
        JobEntry entry(1, cmd, processID);
        entry.isStopped = isStopped;
        this->jobs_list->push_back(entry);
        this->numOfJobs = 1;
    }
    else{
        JobEntry entry(this->getLastJob()->jobID + 1, cmd, processID);
        entry.isStopped = isStopped;
        this->jobs_list->push_back(entry);
        this->numOfJobs++;
    }
}

void JobsList::printJobsList(){
    if(this->jobs_list == nullptr){
        return;
    }

    this->removeFinishedJobs();

    for(auto& entry : *(this->jobs_list)){
        if(entry.job_command != nullptr){
            entry.printJobEntry();
        }
    }
    return;
}

JobsList::JobEntry* JobsList::getJobById(int jobId){
    for(auto& entry : *(this->jobs_list)){
        if(entry.jobID == jobId){
            return &entry;
        }
    }
    return nullptr;
}

void JobsList::removeJobById(int jobId){
    if (this->jobs_list == nullptr || this->jobs_list->empty()) {
        return;
    }
    auto it = this->jobs_list->begin();
    while (it != this->jobs_list->end()) {
        if (it->jobID == jobId) {
            if (it->job_command != nullptr) {
                delete it->job_command;
                it->job_command = nullptr;
            }

            it = this->jobs_list->erase(it);
            this->numOfJobs--;

            return;
        }
        else {
            ++it;
        }
    }
}

JobsList::JobEntry* JobsList::getLastJob(){
    if (this->jobs_list == nullptr || this->jobs_list->empty()) {
        return nullptr;
    }
    return &(this->jobs_list->back());
}

void JobsList::killAllJobs(){
    if(this->jobs_list == nullptr){
        return;
    }

    for(auto& entry : *(this->jobs_list)){
        if(kill(entry.pid, SIGKILL) == -1){
            perror("smash error: kill failed");
            return;
        }
        std::cout << entry.pid << ":" << entry.job_command->printableCommand() << endl;
    }
    return;

}

void JobsList::removeFinishedJobs() {
    if (this->jobs_list == nullptr || this->jobs_list->empty()) {
        return;
    }

    auto it = this->jobs_list->begin();

    while (it != this->jobs_list->end()) {
        int status;

        // Non-blocking check on the job's PID.
        // WNOHANG: Returns 0 immediately if the child has no new state updates.
        // WUNTRACED: Reports if a child process was paused (e.g., Ctrl+Z or SIGSTOP).
        // WCONTINUED: Reports if a paused child was resumed (e.g., SIGCONT).
        pid_t result = waitpid(it->pid, &status, WNOHANG);

        if (result == 0) {
            ++it;
        }
        else if (result == it->pid) {
            if (WIFEXITED(status) || WIFSIGNALED(status)) { //exit or SIGKILLed
                if (it->job_command != nullptr) {
                    delete it->job_command;
                    it->job_command = nullptr;
                }
                it = this->jobs_list->erase(it);
                this->numOfJobs--;

            }

            else {
                ++it;
            }
        }

        else if (result == -1) { //process doesnt exist
            if (it->job_command != nullptr) {
                delete it->job_command;
                it->job_command = nullptr;
            }
            it = this->jobs_list->erase(it);
            this->numOfJobs--;
        }
    }
}

ForegroundCommand::ForegroundCommand(const char *cmd_line) : BuiltInCommand(cmd_line){}

void ForegroundCommand::execute(){
    JobsList::JobEntry* job = nullptr;
    JobsList* jobs = SmallShell::getInstance().getJobsListPointer();

    if(arguments == nullptr || arguments[2] != nullptr){
        std::cerr << "smash error: fg: invalid arguments" <<endl;
    }

    else if(arguments[1] == nullptr){
        if(jobs == nullptr || jobs->numOfJobs == 0){
            std::cerr << "smash error: fg: jobs list is empty" <<endl;
            return;
        }
        job = jobs->getLastJob();
    }


    else{
        int value = this->isNumber(arguments[1]);

        if(value == -1){
            std::cerr << "smash error: fg: invalid arguments" <<endl;
            return;
        }

        job = jobs->getJobById(value);
        if(job == nullptr){
            std::cerr << "smash error: fg: job-id " << value << " does not exist" << endl;
            return;
        }

    }

    if(job != nullptr){
        std::cout << job->job_command->printableCommand()<< " " << job->pid << endl;

        if(kill(job->pid, SIGCONT) < 0){
            perror("smash error: kill failed");
            return;
        }

        SmallShell::getInstance().setCurrentForegroundPID(job->pid);
        int status;

        if(waitpid(job->pid, &status, WUNTRACED) < 0) {
            perror("smash error: waitpid failed");
        }

        if(WIFSTOPPED(status)){
            job->isStopped = true;
        }

        else{
            jobs->removeJobById(job->jobID);
        }

        SmallShell::getInstance().setCurrentForegroundPID(0);
    }

}

KillCommand::KillCommand(const char *cmd_line, JobsList *jobs) : BuiltInCommand(cmd_line){}

void KillCommand::execute() {
    if (arguments[1] == nullptr || arguments[2] == nullptr || arguments[3] != nullptr) {
        std::cerr << "smash error: kill: invalid arguments" << std::endl;
        return;
    }

    std::string sig_str(arguments[1]);
    if (sig_str.empty() || sig_str[0] != '-') {
        std::cerr << "smash error: kill: invalid arguments" << std::endl;
        return;
    }

    std::string sig_num = sig_str.substr(1);
    std::string job_id(arguments[2]);

    if (sig_num.empty() || this->isNumber(sig_num) == -1 || this->isNumber(job_id) == -1) {
        std::cerr << "smash error: kill: invalid arguments" << std::endl;
        return;
    }

    int signum = this->isNumber(sig_num);
    int jobID = this->isNumber(job_id);

    JobsList::JobEntry* job = SmallShell::getInstance().getJobsListPointer()->getJobById(jobID);

    if(job == nullptr){
        std::cerr << "smash error: kill: job-id "<< jobID <<" does not exist" << endl;
        return;
    }

    pid_t pid = job->pid;
    if(kill(pid, signum) == -1){
        perror("smash error: kill failed");
        return;
    }

    if (signum == SIGSTOP || signum == SIGTSTP) {
        job->isStopped = true;
    }
    else if (signum == SIGCONT) {
        job->isStopped = false; // It has resumed running!
    }

    std::cout << "signal number " << signum << " was sent to pid " << pid <<endl;

}

SmallShell::SmallShell() {
    // TODO: add your implementation
}

SmallShell::~SmallShell() {
    // TODO: add your implementation

}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command *SmallShell::CreateCommand(const char *cmd_line) {
    // For example:
    /*
    string cmd_s = _trim(string(cmd_line));
    string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

    if (self->aliases.contains(firstWord)) {
        return aliases[firstWord];
    }

    if (firstWord.compare("pwd") == 0) {
      return new GetCurrDirCommand(cmd_line);
    }
    else if (firstWord.compare("showpid") == 0) {
      return new ShowPidCommand(cmd_line);
    }
    else if ...
    .....
    else {
      return new ExternalCommand(cmd_line);
    }
    */
    return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
    // TODO: Add your implementation here
    // for example:
    // Command* cmd = CreateCommand(cmd_line);
    // cmd->execute();
    // Please note that you must fork smash process for some commands (e.g., external commands....)
}

std::string SmallShell::getPrompt(){
    return this->prompt;
}

void SmallShell::setPrompt(std::string newPrompt){
    this->prompt = newPrompt;
}

void SmallShell::printAliases(){
    for (const auto& pair : this->aliases){
        std::cout << pair.first << "=" << std::string(pair.second->getCmdLine()) << std::endl;
    }
}

void SmallShell::addAlias(std::string alias, Command* cmd){
    this->aliases[alias] = cmd;
}

JobsList* SmallShell::getJobsListPointer(){
    return &this->jobs;
}

bool SmallShell::getIsActive(){
    return this->isActive;
}

void SmallShell::setIsActive(bool isActive){
    this->isActive = isActive;
}