#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <map>
#include <sstream>
#include <iomanip>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <iomanip>
#include <regex>
#include <fcntl.h>
#include <typeinfo>
#include <string>
#include <cstring>
#include "Commands.h"

//I added this to use PATH_MAX
#include <limits.h>


//for disk usage command
struct linux_dirent64 {
    unsigned long  d_ino;
    unsigned long  d_off;
    unsigned short d_reclen;
    unsigned char  d_type;
    char           d_name[];
};

using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";

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

Command::~Command() noexcept {}

//The constructor saves the cmdline, and also saves the arguments and the number of the arguments.
Command::Command(const char *cmd_line) : cmdLine(cmd_line), arguments(new char*[COMMAND_MAX_ARGS]) {
    this->numOfArguments = _parseCommandLine(cmd_line, this->arguments);
}

string Command::printableCommand(){
    string ret;
    for(int i =0; i < numOfArguments; i++){
        ret += " ";
        ret += arguments[i];
    }
    return ret;
}

pid_t Command::getSmashPid() const {
    //getpid is always successful
    return getpid();
}

const char* Command::getCmdLine(){
    return this->cmdLine;
}

int Command::isNumber(string arg){
    bool isNumber = !arg.empty(); 

    for (char c : arg) {
        if (!std::isdigit(c)) {
            isNumber = false;
            break;
        }
    }

    if (isNumber) {
        return std::stoi(arg);
    }

    else{
        return -1;
    }
}

char** Command::removeAmpersandFromArguments() {
    if (this->cmdLine == nullptr) {
        char** empty_args = (char**) malloc(sizeof(char*));
        empty_args[0] = nullptr;
        this->numOfArguments = 0; // Explicitly set to 0 safely
        return empty_args;
    }
    
    char writable_cmd[COMMAND_MAX_LENGTH];
    std::strncpy(writable_cmd, this->cmdLine, sizeof(writable_cmd) - 1);
    writable_cmd[sizeof(writable_cmd) - 1] = '\0';

    const std::string str(writable_cmd);
    if ((str.find(" &") != std::string::npos)) {
        this->numOfArguments--;
    }
    unsigned int idx = str.find_last_not_of(WHITESPACE);

    if (idx != std::string::npos && writable_cmd[idx] == '&') {
        writable_cmd[idx] = ' ';
        writable_cmd[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
    }
    char **clean_args = (char **) malloc(COMMAND_MAX_ARGS * sizeof(char *));
    std::memset(clean_args, 0, COMMAND_MAX_ARGS * sizeof(char *));

    this->numOfArguments = _parseCommandLine(writable_cmd, clean_args);
    return clean_args;
}

char** Command::getArguments(){
    return this->arguments;
}

BuiltInCommand::BuiltInCommand(const char *cmd_line) : Command(cmd_line) {
    this->arguments = this->removeAmpersandFromArguments();
}

EmptyCommand::EmptyCommand(const char *cmd_line) : Command(cmd_line) {}

void EmptyCommand::execute() {}

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

GetCurrDirCommand::GetCurrDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void GetCurrDirCommand::execute(){
    char buffer[PATH_MAX];
    getcwd(buffer, sizeof(buffer));
    std::cout << buffer << std::endl;
}

ChangeDirCommand::ChangeDirCommand(const char *cmd_line, char **plastPwd) :
BuiltInCommand(cmd_line), previousPWD(plastPwd) {}

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

JobsCommand::JobsCommand(const char *cmd_line) : BuiltInCommand(cmd_line){}

void JobsCommand::execute(){
    
}

AliasCommand::AliasCommand(const char *cmd_line) : BuiltInCommand(cmd_line){}

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

UnSetEnvCommand::UnSetEnvCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

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

KillCommand::KillCommand(const char *cmd_line) : BuiltInCommand(cmd_line){}

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

GetFileTypeCommand::GetFileTypeCommand(const char *cmd_line) : BuiltInCommand(cmd_line){}

void GetFileTypeCommand::execute(){

    if (this->numOfArguments > 1) {
        perror("smash error: getfiletype: invalid arguments ");
        return;
    }

    const char* path = this->arguments[0];
    struct stat sb;
    if(lstat(path, &sb) ==-1){
        perror("smash error: getfiletype: invalid arguments ");
        return;
    }

    std::string type;
    if(S_ISREG(sb.st_mode)){
        type = "regular file";
    }

    else if(S_ISDIR(sb.st_mode)){
        type = "directory";
    }

    else if(S_ISCHR(sb.st_mode)){
        type = "character device";
    }

    else if(S_ISBLK(sb.st_mode)){
        type = "block device";
    }

    else if(S_ISFIFO(sb.st_mode)){
        type = "FIFO";
    }

    else if(S_ISLNK(sb.st_mode)){
        type = "symbolic link";
    }

    else if(S_ISSOCK(sb.st_mode)){
        type = "socket";
    }
    else{
        perror("smash error: getfiletype: invalid arguments ");
        return;
    }

    long long size = sb.st_size;

    std::cout<<path<<"'s type is "<< type <<" and takes up " << size << " bytes";
    return;
}

DiskUsageCommand::DiskUsageCommand(const char *cmd_line) : Command(cmd_line), inBackground(false) {
    std::string last_arg(arguments[this->numOfArguments-1]);
    if(!last_arg.empty()){
        //Case 1 - spaces around &, so it is saved as an argument. Change num of arguments.
        if (last_arg == "&") {
            this->inBackground = true;
            free(this->arguments[numOfArguments-1]);
            this->numOfArguments--;
        //Case 2 - /home/student&. Remove the & so the path will be valid.
        } else if (last_arg.back() == '&') {
            this->inBackground = true;
            int index = this->numOfArguments - 1;
            size_t length = strlen(this->arguments[index]);
            this->arguments[index][length-1] = '\0';
        }
    }
}

unsigned long DiskUsageCommand::calculateDiskSpace(const std::string &path) {
    //We will use this struct to get information about the path
    struct stat stFile;
    //With lstat we fill the stFile stat struct
    if (lstat(path.c_str(), &stFile) == -1) {
        perror("smash error: lstat failed");
        return 0;
    }
    //We will sum the number of blocks (blocks are the actual space the file takes on the disk
    //If the current file is a directory, we should traverse and sum it's blocks.
    if (!S_ISDIR(stFile.st_mode)) {
        return stFile.st_blocks;
    }
    //Else, the file is a directory, we need to calculate the disk space of the directory as well
    unsigned long blocksSum = stFile.st_blocks;
    //Make sure we open a directory
    int directoryFD = open(path.c_str(), O_RDONLY | O_DIRECTORY);
    if (directoryFD == -1) {
        perror("smash error: open failed");
        return blocksSum;
    }
    //Buffer to save information from the directory
    char buffer[4096];
    //Keep going until we go over all of the files
    while (true) {
        //With getdents system call we can get information about the file.
        //It does not have a system call format like fork, so we use syscall.
        long bytesRead = syscall(SYS_getdents64, directoryFD, buffer, sizeof(buffer));
        if (bytesRead == -1) {
            perror("smash error: getdents64 failed");
            return blocksSum;
        }
        //We are done reading everything! so we read 0 bytes with getdents.
        if (bytesRead == 0) {
            break;
        }
        //Save the current position - on the buffer all the files' paths are saved
        long currentPosition = 0;
        while (currentPosition < bytesRead) {
            struct linux_dirent64* currentFile = (struct linux_dirent64*) (buffer + currentPosition);
            std::string currentFileName = currentFile->d_name;
            //Skip checking . or .. and avoid calculating every file on the computer!
            if (currentFileName == "." || currentFileName == "..") {
                //Move to the next file. reclen is the length of the current file.
                currentPosition += currentFile->d_reclen;
                continue;
            }
            std::string currentFilePath = path + "/" + currentFileName;
            //Call recursively
            blocksSum += calculateDiskSpace(currentFilePath);
            //Go to the next file
            currentPosition += currentFile->d_reclen;
        }
    }
    if (close(directoryFD) == -1) {
        perror("smash error: close failed");
        return blocksSum;
    }
    return blocksSum;
}

void DiskUsageCommand::execute() {
    if (numOfArguments > 2) {
        std::cerr << "smash error: du: too many arguments" << std::endl;
        return;
    }
    //Will hold the path we need to calculate.
    //We got no arguments, so calculate for the current folder
    if (numOfArguments == 1) {
        pathToDU = '.';
    }
    //We got an argument!
    else {
        pathToDU = arguments[1];
    }
    //If we need to run this in the bg
    if (this->inBackground) {
        pid_t sonPid = fork();
        if (sonPid == -1) {
            perror("smash error: fork failed");
            return;
        }
        //We are the son
        else if (sonPid == 0) {
            setpgrp();
            unsigned long blocksTotal = calculateDiskSpace(pathToDU);
            //Round up the result as required
            unsigned long result = (blocksTotal + 1) / 2;
            std::cout << "Total disk usage: " << result << " KB" << std::endl;
            exit(0);
            //Father. Add job to job list!
        } else {
            Command * copy = new DiskUsageCommand(this->cmdLine);
            SmallShell::getInstance().getJobsListPointer()->addJob(copy, sonPid);
            return;
        }
        //The command should run in the foreground
    } else {
        unsigned long blocksTotal = calculateDiskSpace(pathToDU);
        //Round up the result as required
        unsigned long result = (blocksTotal + 1) / 2;
        std::cout << "Total disk usage: " << result << " KB" << std::endl;
    }
}

ExternalCommand::ExternalCommand(const char *cmd_line) : Command(cmd_line){
    this->background = false;

    if(_isBackgroundComamnd(cmd_line)){
        this->background = true;
    }
}

void ExternalCommand::execute(){
    JobsList* jobs = SmallShell::getInstance().getJobsListPointer();

    string cmd_line = this->printableCommand();
    char* clean_cmd_line = strdup(this->printableCommand().c_str());
    _removeBackgroundSign(clean_cmd_line);
    Command* no_ampersand = new ExternalCommand(clean_cmd_line);
    char** no_amp_args = no_ampersand->getArguments();

    pid_t pid = fork();

    if(pid <0){
        perror("smash error: fork failed");
        if(no_ampersand != nullptr) {
            delete no_ampersand;
        }

        if(clean_cmd_line != nullptr) {
            free(clean_cmd_line);
        }
        return;
    }

    if(pid ==0){ //child
        setpgrp();
        if(cmd_line.find("*") == std::string::npos && cmd_line.find("?") == std::string::npos){//simple command
            if(execvp(no_amp_args[0], no_amp_args) == -1){
                perror("smash error: execvp failed");
                if(no_ampersand != nullptr) {
                    delete no_ampersand;
                }

                if(clean_cmd_line != nullptr) {
                    free(clean_cmd_line);
                }
                _exit(1);
            }
        }
        else{//complex command

            char** bash_args = new char*[4];
            bash_args[0] = strdup("/bin/bash");
            bash_args[1] = strdup("-c");
            bash_args[2] = strdup(clean_cmd_line);
            bash_args[3] = nullptr;

            if(execv("/bin/bash", bash_args) == -1){
                perror("smash error: execv failed");
                this->freeArray(bash_args);
                //this->freeArray(args_execvp);
                _exit(1);
            }
        }
    }

    else{//father
        if(this->background){
            if(jobs != nullptr){
                Command* copy = new ExternalCommand(this->cmdLine);
                jobs->addJob(copy, pid);
            }

            int status;
            if(waitpid(pid, &status, WNOHANG)< 0){
                perror("smash error: waitpid failed");
            }

        }

        else{
            SmallShell::getInstance().setCurrentForegroundPID(pid);

            int status;
            if (waitpid(pid, &status, WUNTRACED) < 0) {
                perror("smash error: waitpid failed");
            }

            if (WIFSTOPPED(status)) {
                if (jobs != nullptr) {
                    Command* copy = new ExternalCommand(this->cmdLine);
                    jobs->addJob(copy, pid);

                    // Mark its state status as explicitly stopped in our tracking database
                    int last_id = jobs->getLastJob()->jobID;
                    JobsList::JobEntry* entry = jobs->getJobById(last_id);
                    if (entry != nullptr) {
                        entry->isStopped = true;
                    }
                }
            }

            SmallShell::getInstance().setCurrentForegroundPID(0);
        }

        if(no_ampersand != nullptr) {
            delete no_ampersand;
        }

        if(clean_cmd_line != nullptr) {
            free(clean_cmd_line);
        }

    }

}

void ExternalCommand::freeArray(char** array_to_delete) {
    if (array_to_delete == nullptr) {
        return;
    }

    int i = 0;
    while (array_to_delete[i] != nullptr) {
        free(array_to_delete[i]);
        i++;
    }

    free(array_to_delete);
}


SmallShell::SmallShell() {
    this->promptMessage = new char[COMMAND_MAX_LENGTH];
    strcpy(this->promptMessage, "smash");
    this->existing_aliases = new std::list<std::pair<std::string, std::string>>();
    this->jobs = new JobsList();
}

SmallShell::~SmallShell() {
    // TODO: add your implementation
    delete this->OLDPWD;
    delete this->promptMessage;
    delete this->existing_aliases;
    delete this->jobs;
}


/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command *SmallShell::CreateCommand(const char *cmd_line) {
    string cmd_s = _trim(string(cmd_line));
    string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

    if (firstWord.compare("alias") == 0 || firstWord.compare("alias&") == 0) {
        return new AliasCommand(cmd_line);
    }
    string expanded_cmd_str ="";

    if (existing_aliases != nullptr && !existing_aliases->empty() && firstWord.compare("alias") != 0) {
        if(isAlias(firstWord)){
            string alias_value = getAliasCmdByName(firstWord);
            string rest_of_line = "";

            size_t first_space_pos = cmd_s.find_first_of(" \n");
            if(first_space_pos != string::npos){
                rest_of_line = cmd_s.substr(first_space_pos);
            }

            expanded_cmd_str = alias_value + rest_of_line;
            cmd_line = expanded_cmd_str.c_str();
            cmd_s = _trim(expanded_cmd_str);
            firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));
        }
    }

    if (cmd_s.empty()) {
        return new EmptyCommand(cmd_line);
    }
    /*
    //We found < so we have either < or <<. Either way, it's I/O redirection.
    if (cmd_s.find(">>") != std::string::npos) {
        return new RedirectionCommand(cmd_line, true);
    }
    else if (cmd_s.find('>') != std::string::npos) {
        return new RedirectionCommand(cmd_line, false);
    }
    
    
    //We found | so we have either | or |&. Either way, it's Pipe command.
    if (cmd_s.find("|&") != std::string::npos) {
        return new PipeCommand(cmd_line, 2, true);
    }
    else if (cmd_s.find('|') != std::string::npos) {
        return new PipeCommand(cmd_line, 1, false);
    }
    */

    if (firstWord.compare("chprompt") == 0 || firstWord.compare("chprompt&") == 0 || firstWord.compare("chprompt &") == 0){
        //
    }
    else if (firstWord.compare("showpid") == 0 || firstWord.compare("showpid&") == 0 || firstWord.compare("showpid &") == 0){
        return new ShowPidCommand(cmd_line);
    }
    else if (firstWord.compare("pwd") == 0 || firstWord.compare("pwd&") == 0) {
        return new GetCurrDirCommand(cmd_line);
    }
    //& can come after the last argument (if there are any)
    else if (firstWord.compare("cd") == 0 || firstWord.compare("cd&") == 0) {
      return new ChangeDirCommand(cmd_line, this->oldPwdPointer());
    }
    else if (firstWord.compare("unsetenv") == 0 || firstWord.compare("unsetenv&") == 0) {
        return new UnSetEnvCommand(cmd_line);
    }

    else if (firstWord.compare("unalias") == 0) {
        return new UnAliasCommand(cmd_line);
    }
    else if (firstWord.compare("jobs") == 0 || firstWord.compare("jobs&") == 0) {
        //
    }
    else if (firstWord.compare("kill") == 0) {
        return new KillCommand(cmd_line);
    }
    else if (firstWord.compare("quit") == 0 || firstWord.compare("quit&") == 0) {
        //;
    }
    else if (firstWord.compare("fg") == 0) {
        return new ForegroundCommand(cmd_line);
    }
    else if (firstWord.compare("du") == 0 || firstWord.compare("du&") == 0) {
        return new DiskUsageCommand(cmd_line);
    }
    /*
    else if (firstWord.compare("whoami") == 0 || firstWord.compare("whoami&") == 0) {
        return new WhoAmICommand(cmd_line);
    }
    */
    else {
        return new ExternalCommand(cmd_line);
    }

    return nullptr;
}


void SmallShell::executeCommand(const char *cmd_line) {
    this->jobs->removeFinishedJobs();
    Command* cmd = CreateCommand(cmd_line);
    if(cmd != nullptr){
        cmd->execute();
        delete cmd;
    }
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

//We return the address of the OLDPWD variable, so we could write to it and change it.
char** SmallShell::oldPwdPointer() {
    return &(this->OLDPWD);
}

//Return the prompt pointer so we could modify it outside the class.
char* SmallShell::getPromptMessage() const {
    return this->promptMessage;
}

void SmallShell::setCurrentForegroundPID(int pid){
    this->foregroundPID = pid;
}

int SmallShell::getCurrentForegroundPID(){
    return this->foregroundPID;
}

JobsList* SmallShell::getJobsListPointer() {
    return this->jobs;
}

bool SmallShell::isAlias(std::string name){
    for(const auto& pair :*this->existing_aliases){
        if(name == pair.first){
            return true;
        }
    }
    return false;
}

void SmallShell::removeAlias(const std::string& key_to_remove) {
    if (existing_aliases == nullptr || existing_aliases->empty()) {
        return;
    }

    auto it = existing_aliases->begin();

    while (it != existing_aliases->end()) {
        if (it->first == key_to_remove) {
            it = existing_aliases->erase(it);
            break;
        }
        else {
            ++it;
        }
    }
}

string SmallShell::getAliasCmdByName(string name){
    if (existing_aliases == nullptr || existing_aliases->empty()) {
        return "";
    }

    for(auto i = existing_aliases->begin(); i != existing_aliases->end(); ++i){
        if(i->first.compare(name) == 0){
            return i->second;
        }

    }
    return "";
}

list<pair<string, string>>* SmallShell::getExistingAliases(){
    return this->existing_aliases;
}