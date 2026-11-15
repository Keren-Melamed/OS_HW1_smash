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

pid_t Command::getSmashPid() const {
    //getpid is always successful
    return getpid();
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