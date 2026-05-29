#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"
#include <climits>
#include <regex>
#include <fcntl.h>
#include <time.h>
#include <ftw.h>
#include <sys/syscall.h>

using namespace std;

extern char **__environ;

const std::string WHITESPACE = " \n\r\t\f\v";

const vector<string> AliasesList::reservedKeywords = {
    "chprompt", "showpid", "pwd", "cd", "jobs", "fg", "quit", "kill", "alias", "unalias", "unsetenv", "sysinfo", "du", "whoami", "usbinfo"
};

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

size_t _findOutsideQuotes(const string& str, const string& symbol){
    bool insideQuotes = false;
    for(size_t pos = 0; pos < str.length(); pos++){
        if(str[pos] == '\''){
            insideQuotes = !insideQuotes;
        }
        if(!insideQuotes && str.substr(pos, symbol.length()) == symbol){
            return pos;
        }
    }
    return string::npos;
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
    size_t index = str.find_last_not_of(WHITESPACE);
    if(index == string::npos){
        return false;
    }
    return str[index] == '&';
}

string _copyWithoutBackgroundSign(const string& cmd_line){
    string copy = cmd_line;
    size_t idx = copy.find_last_not_of(WHITESPACE);
    if(idx == string::npos || copy[idx] != '&') {
        return cmd_line;
    }
    copy[idx] = ' ';
    return _rtrim(copy);
}

string _parseAliasCommandLine(const std::string& cmd_line){
    string cmd_s = _trim(string((cmd_line)));
    string delimeters = WHITESPACE + "|>&";
    size_t delimeterIndex = cmd_s.find_first_of(delimeters);
    if(delimeterIndex == string::npos){
        delimeterIndex = cmd_s.length();
    }
    string firstWord = cmd_s.substr(0, delimeterIndex);
    string aliasCommand = SmallShell::getInstance().getAliasesList().getCommandFromAlias(firstWord);
    if(aliasCommand.empty()){
        return cmd_line;
    }
    return aliasCommand + cmd_s.substr(delimeterIndex);
}

string _readFileToString(const string& path, bool printErrors){
    int fd = open(path.c_str(), O_RDONLY);
    if(fd == -1){
        if(printErrors){
            perror("smash error: open failed");
        }
        return "";
    }
    string res = "";
    char buff[256];
    ssize_t bytesRead;
    while((bytesRead = read(fd, buff, sizeof(buff))) > 0){
        res.append(buff, bytesRead);
    }
    if(close(fd) == -1){
        if(printErrors){
            perror("smash error: close failed");
        }
        return "";
    }
    if(bytesRead == -1){
        if(printErrors){
            perror("smash error: read failed");
        }
        return "";
    }
    return res;
}

Command::Command(const char *cmd_line) : cmd_line(cmd_line) {}

const char* Command::getCmdLine() const {
    return cmd_line;
}

string Command::getAliasCmdLine() const {
    return _parseAliasCommandLine(getCmdLine());
}

int Command::getAliasArgs(char **args) const {
    return _parseCommandLine(_copyWithoutBackgroundSign(getAliasCmdLine()).c_str(), args);
}

bool Command::isBackgroundable() const {
    return false;
}

BuiltInCommand::BuiltInCommand(const char *cmd_line) : Command(cmd_line) {}

ExternalCommand::ExternalCommand(const char *cmd_line) : Command(cmd_line) {}

bool ExternalCommand::isBackgroundable() const {
    return true;
}

SimpleExternalCommand::SimpleExternalCommand(const char *cmd_line) : ExternalCommand(cmd_line) {}

void SimpleExternalCommand::execute() {
    char* args[COMMAND_MAX_ARGS];
    int argc = getAliasArgs(args);
    if(argc == 0){
        exit(1);
    }
    execvp(args[0], args);
    perror("smash error: execvp failed");
    exit(1);
}

ComplexExternalCommand::ComplexExternalCommand(const char *cmd_line) : ExternalCommand(cmd_line) {}

void ComplexExternalCommand::execute() { // check casting
    string aliasCommandLine = _copyWithoutBackgroundSign(getAliasCmdLine());
    char *args[4];
    args[0] = (char*)"/bin/bash";
    args[1] = (char*)"-c";
    args[2] = (char*)aliasCommandLine.c_str();
    args[3] = nullptr;
    execv("/bin/bash", args);
    perror("smash error: execv failed");
    exit(1);
}

ChangePromptCommand::ChangePromptCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void ChangePromptCommand::execute() {
    char* args[COMMAND_MAX_ARGS];
    int argc = getAliasArgs(args);
    if(argc > 1){
        SmallShell::getInstance().setPrompt(std::string(args[1]) + "> ");
    }else{
        SmallShell::getInstance().setPrompt("smash> ");
    }
}

ShowPidCommand::ShowPidCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void ShowPidCommand::execute() {
    pid_t pid = getpid();
    cout << "smash pid is " << pid << endl;
}

GetCurrDirCommand::GetCurrDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void GetCurrDirCommand::execute() {
    char buff[PATH_MAX];
    if(getcwd(buff, PATH_MAX) != nullptr){
        cout << buff << endl;
        return;
    }
    perror("smash error: getcwd failed");
}

ChangeDirCommand::ChangeDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void ChangeDirCommand::execute() {
    char* args[COMMAND_MAX_ARGS];
    int argc = getAliasArgs(args);
    if(argc > 2){
        cerr << "smash error: cd: too many arguments" << endl;
        return;
    }else if(argc == 2){
        string nextPwd = string(args[1]);
        if(nextPwd == "-"){
            nextPwd = SmallShell::getInstance().getPlastPwd();
            if(nextPwd.empty()){
                cerr << "smash error: cd: OLDPWD not found" << endl;
                return;
            }
        }
        char plastPwd[PATH_MAX];
        if(getcwd(plastPwd, PATH_MAX) == nullptr){
            perror("smash error: getcwd failed");
            return;
        }
        if(chdir(nextPwd.c_str()) == 0){
            SmallShell::getInstance().setPlastPwd(string(plastPwd));
        }else{
            perror("smash error: chdir failed");
            return;
        }
    }
}

JobsCommand::JobsCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void JobsCommand::execute() {
    SmallShell::getInstance().getJobsList().printJobsListByJid();
}

ForegroundCommand::ForegroundCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void ForegroundCommand::execute() {
    char* args[COMMAND_MAX_ARGS];
    int argc = getAliasArgs(args);
    if(argc > 2){
        cerr << "smash error: fg: invalid arguments" << endl;
        return;
    }
    JobsList& jobs = SmallShell::getInstance().getJobsList();
    JobsList::JobEntry *job = nullptr;
    try{
        if(argc == 2){
            int jobid = std::stoi(string(args[1]));
            job = jobs.getJobById(jobid);
            if(job == nullptr){
                cerr << "smash error: fg: job-id " << jobid << " does not exist" << endl;
                return;
            }
        }else{
            job = jobs.getLastJob();
            if(job == nullptr){
                cerr << "smash error: fg: jobs list is empty" << endl;
                return;
            }
        }
        pid_t foregroundPid = job->getJobPid();
        cout << job->getCmdLine() << " " << foregroundPid << endl;
        SmallShell::getInstance().setForegroundPid(foregroundPid);
        jobs.removeJobById(job->getJid());
        if(waitpid(foregroundPid, nullptr, 0) == -1){
            perror("smash error: waitpid failed");
            return;
        }
        SmallShell::getInstance().setForegroundPid(-1);
    } catch(const exception& e){
        cerr << "smash error: fg: invalid arguments" << endl;
        return;
    }
}

QuitCommand::QuitCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void QuitCommand::execute(){
    char* args[COMMAND_MAX_ARGS];
    int argc = getAliasArgs(args);
    if(argc > 1 && string(args[1]) == "kill"){
        JobsList& jobs = SmallShell::getInstance().getJobsList();
        cout << "smash: sending SIGKILL signal to " << jobs.getSize() << " jobs:" << endl;
        jobs.killAllJobs();
        jobs.printJobsListByPid();
    }
    exit(0);
}

KillCommand::KillCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void KillCommand::execute(){
    char* args[COMMAND_MAX_ARGS];
    int argc = getAliasArgs(args);
    if(argc == 3 && string(args[1])[0] == '-'){
        try{
            int signum = std::stoi(string(args[1]).substr(1));
            int jobid = std::stoi(string(args[2]));
            JobsList::JobEntry *job = SmallShell::getInstance().getJobsList().getJobById(jobid);
            if(job != nullptr){
                if(kill(job->getJobPid(), signum) == -1){
                    perror("smash error: kill failed");
                }else{
                    cout << "signal number " << signum << " was sent to pid " << job->getJobPid() << endl;
                }
            }else{
                cerr << "smash error: kill: job-id " << jobid << " does not exist" << endl;
            }
        } catch(const exception& e){
            cerr << "smash error: kill: invalid arguments" << endl;
        }
    }else{
        cerr << "smash error: kill: invalid arguments" << endl;
    }
}

AliasCommand::AliasCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void AliasCommand::execute(){
    string commandLine = _trim(_copyWithoutBackgroundSign(getCmdLine()).c_str());
    AliasesList& aliases = SmallShell::getInstance().getAliasesList();
    if(commandLine == "alias"){
        aliases.printAliases();
        return;
    }
    regex pattern("^alias ([a-zA-Z0-9_]+)='([^']*)'$");
    smatch match;
    if(regex_match(commandLine, match, pattern)){
        string alias = match[1];
        string command = match[2];
        if(aliases.isValidAlias(alias)){
            aliases.addAlias(alias, command);
        }else{
            cerr << "smash error: alias: " << alias << " already exists or is a reserved command" << endl;
        }
    }else{
        cerr << "smash error: alias: invalid alias format" << endl;
    }
}

UnAliasCommand::UnAliasCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void UnAliasCommand::execute(){
    char* args[COMMAND_MAX_ARGS];
    int argc = getAliasArgs(args);
    if(argc == 1){
        cerr << "smash error: unalias: not enough arguments" << endl;
    }else{
        AliasesList& aliases = SmallShell::getInstance().getAliasesList();
        for(int i = 1; i < argc; i++){
            string alias = string(args[i]);
            if(aliases.getCommandFromAlias(alias).empty()){
                cerr << "smash error: unalias: " << alias << " alias does not exist" << endl;
                return;
            }
            aliases.removeAlias(alias);
        }
    }
}

UnSetEnvCommand::UnSetEnvCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void UnSetEnvCommand::execute(){
    char* args[COMMAND_MAX_ARGS];
    int argc = getAliasArgs(args);
    if(argc == 1){
        cerr << "smash error: unsetenv: not enough arguments" << endl;
        return;
    }
    pid_t pid = getpid();
    string path = "/proc/" + to_string(pid) + "/environ";
    int fd = open(path.c_str(), O_RDONLY);
    if(fd == -1){
        perror("smash error: open failed");
        return;
    }
    vector<string> environVars;
    string currentVar = "";
    char buff[4096];
    ssize_t bytesRead;
    while((bytesRead = read(fd, buff, 4096)) > 0){ // check the last one if not ending with \0
        for(int byte = 0; byte < bytesRead; byte++){
            if(buff[byte] == '\0'){
                environVars.push_back(currentVar.substr(0, currentVar.find_first_of("=")));
                currentVar = "";
            }else{
                currentVar += buff[byte];
            }
        }
    }
    if(close(fd) == -1){
        perror("smash error: close failed");
        return;
    }
    for(int varIndex = 1; varIndex < argc; varIndex++){
        string var = string(args[varIndex]);
        bool found = false;
        for(const string& environVar : environVars){
            if(var == environVar){
                found = true;
                for(int i = 0; environ[i] != nullptr; i++){
                    if(string(environ[i]).find(var + "=") == 0){
                        for(int j = i; environ[j] != nullptr; j++){
                            environ[j] = environ[j+1];
                        }
                        break;
                    }
                }
            }
        }
        if(!found){
            cerr << "smash error: unsetenv: " << var << " does not exist" << endl;
            return;
        }
    }
}

SysInfoCommand::SysInfoCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void SysInfoCommand::execute(){
    string system = _rtrim(_readFileToString("/proc/sys/kernel/ostype", true));
    string hostname = _rtrim(_readFileToString("/proc/sys/kernel/hostname", true));
    string osrelease = _rtrim(_readFileToString("/proc/sys/kernel/osrelease", true));
    string uptime = _rtrim(_readFileToString("/proc/uptime", true));
    double uptimeSec = 0;
    istringstream stream(uptime);
    stream >> uptimeSec;
    time_t bootSec = time(NULL) - uptimeSec;
    const struct tm *bootSecTM = localtime(&bootSec);
    char bootBuff[128];
    strftime(bootBuff, sizeof(bootBuff), "%Y-%m-%d %H:%M:%S", bootSecTM);
    cout << "System: " << system << endl;
    cout << "Hostname: " << hostname << endl;
    cout << "Kernel: " << osrelease << endl;
    cout << "Architecture: x86_64" << endl;
    cout << "Boot Time: " << bootBuff << endl;
}

JobsList::JobEntry::JobEntry(int jid, pid_t jobPid, const std::string& cmdLine) : jid(jid), jobPid(jobPid), cmdLine(cmdLine) {}

int JobsList::JobEntry::getJid() const {
    return jid;
}

pid_t JobsList::JobEntry::getJobPid() const {
    return jobPid;
}

string JobsList::JobEntry::getCmdLine() const {
    return cmdLine;
}

int JobsList::getSize() const{
    return list.size();
}

void JobsList::addJob(const Command& cmd, pid_t jobPid){
    removeFinishedJobs();
    int jid = (!list.empty()) ? getLastJob()->getJid() + 1 : 1;
    list.push_back(JobEntry(jid, jobPid, string(cmd.getCmdLine())));
}

void JobsList::printJobsListByJid(){
    removeFinishedJobs();
    for(const JobEntry& job : list){
        cout << "[" << job.getJid() << "] " << job.getCmdLine() << endl;
    }
}

void JobsList::printJobsListByPid(){ // maybe removeFinishedJobs() before
    for(const JobEntry& job : list){
        cout << job.getJobPid() << ": " << job.getCmdLine() << endl;
    }
}

void JobsList::killAllJobs(){
    for(const JobEntry& job : list){
        if(kill(job.getJobPid(), SIGKILL) == -1){
            perror("smash error: kill failed");
            return;
        }
    }
}

void JobsList::removeFinishedJobs(){
    auto it = list.begin();
    while(it != list.end()){
        if(waitpid(it->getJobPid(), nullptr, WNOHANG) > 0){
            it = list.erase(it);
        }else{
            it++;
        }
    }
}

JobsList::JobEntry *JobsList::getJobById(int jobId) {
    for(JobEntry& job : list){
        if(job.getJid() == jobId){
            return &job;
        }
    }
    return nullptr;
}

void JobsList::removeJobById(int jobId){
    for(auto it = list.begin(); it != list.end(); it++){
        if(it->getJid() == jobId){
            list.erase(it);
            return;
        }
    }
}

JobsList::JobEntry *JobsList::getLastJob() {
    return (!list.empty()) ? &list.back() : nullptr;
}

string AliasesList::getCommandFromAlias(const std::string& alias) const {
    for(const AliasEntry& aliasEntry : aliases){
        if(aliasEntry.alias == alias){
            return aliasEntry.command;
        }
    }
    return "";
}

void AliasesList::addAlias(const std::string& alias, const std::string& command){
    aliases.push_back(AliasEntry{alias, command});
}

bool AliasesList::removeAlias(const std::string& alias){
    for(auto it = aliases.begin(); it != aliases.end(); it++){
        if(it->alias == alias){
            aliases.erase(it);
            return true;
        }
    }
    return false;
}

void AliasesList::printAliases() const{
    for(const AliasEntry& aliasEntry : aliases){
        cout << aliasEntry.alias << "='" << aliasEntry.command << "'" << endl;
    }
}

bool AliasesList::isValidAlias(const std::string& alias) const {
    if(!getCommandFromAlias(alias).empty()){
        return false;
    }
    for(const string& reservedKeyword : reservedKeywords){
        if(reservedKeyword == alias){
            return false;
        }
    }
    return true;
}

SmallShell::SmallShell() : prompt("smash> "), foregroundPid(-1) {}

Command *SmallShell::CreateCommand(const char *cmd_line) {
    string aliasCommand = _trim(_parseAliasCommandLine(cmd_line));
    if(_findOutsideQuotes(aliasCommand, ">") != string::npos){
        return new RedirectionCommand(cmd_line);
    }else if(_findOutsideQuotes(aliasCommand, "|") != string::npos){
        return new PipeCommand(cmd_line);
    }
    string aliasCommandWithoutBG = _copyWithoutBackgroundSign(aliasCommand.c_str());
    string firstWord = aliasCommandWithoutBG.substr(0, aliasCommandWithoutBG.find_first_of(WHITESPACE));
    if(firstWord.compare("chprompt") == 0){
        return new ChangePromptCommand(cmd_line);
    }else if(firstWord.compare("showpid") == 0){
        return new ShowPidCommand(cmd_line);
    }else if(firstWord.compare("pwd") == 0){
        return new GetCurrDirCommand(cmd_line);
    }else if(firstWord.compare("cd") == 0){
        return new ChangeDirCommand(cmd_line);
    }else if(firstWord.compare("jobs") == 0){
        return new JobsCommand(cmd_line);
    }else if(firstWord.compare("fg") == 0){
        return new ForegroundCommand(cmd_line);
    }else if(firstWord.compare("quit") == 0){
        return new QuitCommand(cmd_line);
    }else if(firstWord.compare("kill") == 0){
        return new KillCommand(cmd_line);
    }else if(firstWord.compare("alias") == 0){
        return new AliasCommand(cmd_line);
    }else if(firstWord.compare("unalias") == 0){
        return new UnAliasCommand(cmd_line);
    }else if(firstWord.compare("unsetenv") == 0){
        return new UnSetEnvCommand(cmd_line);
    }else if(firstWord.compare("sysinfo") == 0){
        return new SysInfoCommand(cmd_line);
    }else if(firstWord.compare("du") == 0){
        return new DiskUsageCommand(cmd_line);
    }else if(firstWord.compare("whoami") == 0){
        return new WhoAmICommand(cmd_line);
    }else if(firstWord.compare("usbinfo") == 0){
        return new USBInfoCommand(cmd_line);
    }else if(string(cmd_line).find("*") == string::npos && string(cmd_line).find("?") == string::npos){
        return new SimpleExternalCommand(cmd_line);
    }else{
        return new ComplexExternalCommand(cmd_line);
    }
}

void SmallShell::executeCommand(const char *cmd_line) {
    Command* cmd = CreateCommand(cmd_line);
    if(cmd == nullptr){
        return;
    }
    jobs.removeFinishedJobs();
    if(cmd->isBackgroundable()){
        pid_t jobPid = fork();
        if(jobPid == -1){
            perror("smash error: fork failed");
            return;
        }
        if(jobPid == 0){
            if(setpgrp() == -1){
                perror("smash error: setpgrp failed");
                exit(1);
            }
            cmd->execute();
            exit(0);
        }else{
            if(_isBackgroundComamnd(cmd->getAliasCmdLine().c_str())){
                jobs.addJob(*cmd, jobPid);
            }else{
                foregroundPid = jobPid;
                if(waitpid(jobPid, nullptr, 0) == -1){
                    perror("smash error: waitpid failed");
                    return;
                }
                foregroundPid = -1;
            }
        }
    }
    else{
        cmd->execute();
    }
}

string SmallShell::getPrompt() const {
    return prompt;
}

void SmallShell::setPrompt(const std::string& prompt) {
    this->prompt = prompt;
}

string SmallShell::getPlastPwd() const {
    return plastPwd;
}

void SmallShell::setPlastPwd(const std::string& plastPwd) {
    this->plastPwd = plastPwd;
}

pid_t SmallShell::getForegroundPid() const {
    return foregroundPid;
}

void SmallShell::setForegroundPid(pid_t foregroundPid) {
    this->foregroundPid = foregroundPid;
}

JobsList& SmallShell::getJobsList() {
    return jobs;
}

AliasesList& SmallShell::getAliasesList() {
    return aliases;
}

RedirectionCommand::RedirectionCommand(const char *cmd_line) : Command(cmd_line) {}

void RedirectionCommand::execute(){
    string commandLine = getAliasCmdLine();
    string  inputCommand = "", outputFile = "";
    bool isAppend = false;
    size_t IOSignIndex;
    if((IOSignIndex = _findOutsideQuotes(commandLine, ">>")) != string::npos){
        isAppend = true;
        inputCommand = commandLine.substr(0, IOSignIndex);
        outputFile = commandLine.substr(IOSignIndex + 2);
    }else if((IOSignIndex = _findOutsideQuotes(commandLine, ">")) != string::npos){
        inputCommand = commandLine.substr(0, IOSignIndex);
        outputFile = commandLine.substr(IOSignIndex + 1);
    }else{
        return;
    }
    inputCommand = _trim(_copyWithoutBackgroundSign(inputCommand));
    outputFile = _trim(_copyWithoutBackgroundSign(outputFile));
    int openFlags = O_WRONLY | O_CREAT | (isAppend ? O_APPEND : O_TRUNC);
    int newOutputFd = open(outputFile.c_str(), openFlags, 0666);
    if(newOutputFd == -1){
        perror("smash error: open failed");
        return;
    }
    int oldOutputFdCopy = dup(1);
    if(oldOutputFdCopy == -1){
        perror("smash error: dup failed");
        close(newOutputFd);
        return;
    }
    if(dup2(newOutputFd, 1) == -1){
        perror("smash error: dup2 failed");
        close(newOutputFd);
        close(oldOutputFdCopy);
        return;
    }
    SmallShell::getInstance().executeCommand(inputCommand.c_str());
    if(dup2(oldOutputFdCopy, 1) == -1){
        perror("smash error: dup2 failed");
        close(newOutputFd);
        close(oldOutputFdCopy);
        return;
    }
    if(close(newOutputFd) == -1){
        perror("smash error: close failed");
        close(oldOutputFdCopy);
        return;
    }
    if(close(oldOutputFdCopy) == -1){
        perror("smash error: close failed");
        return;
    }
}

PipeCommand::PipeCommand(const char *cmd_line) : Command(cmd_line) {}

void PipeCommand::execute(){
    string commandLine = getAliasCmdLine();
    string  command1str = "", command2str = "";
    bool isErrPipe = false;
    size_t pipeSignIndex;
    if((pipeSignIndex = _findOutsideQuotes(commandLine, "|&")) != string::npos){
        isErrPipe = true;
        command1str = commandLine.substr(0, pipeSignIndex);
        command2str = commandLine.substr(pipeSignIndex + 2);
    }else if((pipeSignIndex = _findOutsideQuotes(commandLine, "|")) != string::npos){
        command1str = commandLine.substr(0, pipeSignIndex);
        command2str = commandLine.substr(pipeSignIndex + 1);
    }else{
        return;
    }
    command1str = _trim(_copyWithoutBackgroundSign(command1str));
    command2str = _trim(_copyWithoutBackgroundSign(command2str));
    int fd[2];
    if(pipe(fd) == -1){
        perror("smash error: pipe failed");
        return;
    }
    pid_t pid1 = fork();
    if(pid1 == -1){
        perror("smash error: fork failed");
        close(fd[0]);
        close(fd[1]);
        return;
    }
    if(pid1 == 0){
        if(setpgrp() == -1){
                perror("smash error: setpgrp failed");
                exit(1);
            }
        if(dup2(fd[1], (isErrPipe ? 2 : 1)) == -1){
            perror("smash error: dup2 failed");
            exit(1);
        }
        if((close(fd[0]) == -1) || (close(fd[1]) == -1)){
            perror("smash error: close failed");
            exit(1);
        }
        Command* cmd1 = SmallShell::getInstance().CreateCommand(command1str.c_str());
        if(cmd1 == nullptr){
            exit(1);
        }
        cmd1->execute();
        exit(0);
    }
    pid_t pid2 = fork();
    if(pid2 == -1){
        perror("smash error: fork failed");
        close(fd[0]);
        close(fd[1]);
        return;
    }
    if(pid2 == 0){
        if(setpgrp() == -1){
                perror("smash error: setpgrp failed");
                exit(1);
            }
        if(dup2(fd[0], 0) == -1){
            perror("smash error: dup2 failed");
            exit(1);
        }
        if((close(fd[0]) == -1) || (close(fd[1]) == -1)){
            perror("smash error: close failed");
            exit(1);
        }
        Command* cmd2 = SmallShell::getInstance().CreateCommand(command2str.c_str());
        if(cmd2 == nullptr){
            exit(1);
        }
        cmd2->execute();
        exit(0);
    }
    bool close0Failed = (close(fd[0]) == -1);
    if((close(fd[1]) == -1) || close0Failed){
        perror("smash error: close failed");
        return;
    }
    if(waitpid(pid1, nullptr, 0) == -1){
        perror("smash error: waitpid failed");
        return;
    }
    if(waitpid(pid2, nullptr, 0) == -1){
        perror("smash error: waitpid failed");
        return;
    }
}

DiskUsageCommand::DiskUsageCommand(const char *cmd_line) : Command(cmd_line) {}

int DiskUsageCommand::total_du = 0;

int DiskUsageCommand::nftwCallback(const char *fpath, const struct stat *statsBuffer, int additionalInfo, struct FTW *ftw){
    total_du += ((statsBuffer->st_blocks + 1) / 2);
    return 0;
}

bool DiskUsageCommand::isBackgroundable() const {
    return true;
}

void DiskUsageCommand::execute(){
    total_du = 0;
    char* args[COMMAND_MAX_ARGS];
    int argc = getAliasArgs(args);
    string path;
    if(argc == 1){
        char buff[PATH_MAX];
        if(getcwd(buff, PATH_MAX) == nullptr){
            perror("smash error: getcwd failed");
            return;
        }
        path = string(buff);
    }else if(argc == 2){
        path = string(args[1]);
    }else{
        cerr << "smash error: du: too many arguments" << endl;
        return;
    }
    if(nftw(path.c_str(), nftwCallback, 20, FTW_PHYS) == -1){
        return;
    }
    cout << "Total disk usage: " << total_du << " KB" << endl;
}

WhoAmICommand::WhoAmICommand(const char *cmd_line) : Command(cmd_line) {}

bool WhoAmICommand::isBackgroundable() const {
    return true;
}

void WhoAmICommand::execute(){
    string uid = to_string(getuid());
    string gid = to_string(getgid());
    int fd = open("/etc/passwd", O_RDONLY);
    if(fd == -1){
        perror("smash error: open failed");
        return;
    }
    string currentUser = "";
    char buff[4096];
    ssize_t bytesRead;
    while((bytesRead = read(fd, buff, 4096)) > 0){ // check the last one if not ending with \n
        for(int byte = 0; byte < bytesRead; byte++){
            if(buff[byte] == '\n'){
                istringstream stream(currentUser);
                string token;
                vector<string> userFields;
                while(getline(stream, token, ':')){
                    userFields.push_back(token);
                }
                if(userFields.size() >= 6){
                    if(userFields[2] == uid){
                        cout << userFields[0] << endl;
                        cout << uid << endl;
                        cout << gid << endl;
                        cout << userFields[5] << endl;
                        if(close(fd) == -1){
                            perror("smash error: close failed");
                            return;
                        }
                        return;
                    }
                }
                currentUser = "";
            }else{
                currentUser += buff[byte];
            }
        }
    }
    if(close(fd) == -1){
        perror("smash error: close failed");
        return;
    }
}

USBInfoCommand::USBInfoCommand(const char *cmd_line) : Command(cmd_line) {}

bool USBInfoCommand::isBackgroundable() const {
    return true;
}

void USBInfoCommand::execute(){
    string usbDirPath = "/sys/bus/usb/devices/";
    int fd = open(usbDirPath.c_str(), O_RDONLY | O_DIRECTORY);
    if(fd == -1){
        perror("smash error: open failed");
        return;
    }
    char buff[4096];
    int bytesRead, devnum = 0;
    while((bytesRead = syscall(SYS_getdents64, fd, buff, 4096)) > 0){
        int bytePos = 0;
        while(bytePos < bytesRead){
            linux_dirent64 *dirent = (linux_dirent64 *) (buff + bytePos);
            string dirName = dirent->d_name;
            bytePos += dirent->d_reclen;
            if(dirName == "." || dirName == ".."){
                continue;
            }
            devnum++;
            string vendorPath = usbDirPath + dirName + "/idVendor";
            string productPath = usbDirPath + dirName + "/idProduct";
            string manufacturerPath = usbDirPath + dirName + "/manufacturer";
            string maxPowerPath = usbDirPath + dirName + "/bMaxPower";
            string vendor = _rtrim(_readFileToString(vendorPath, false));
            string product = _rtrim(_readFileToString(productPath, false));
            string manufacturer = _rtrim(_readFileToString(manufacturerPath, false));
            string maxPower = _rtrim(_readFileToString(maxPowerPath, false));
            if(vendor.empty()){
                vendor = "N/A";
            }
            if(product.empty()){
                product = "N/A";
            }
            if(manufacturer.empty()){
                manufacturer = "N/A";
            }
            if(maxPower.empty()){
                maxPower = "N/AmA";
            }
            cout << "Device " << devnum << ": ID " << vendor << ":" << product << " " << manufacturer << " " << dirName << " MaxPower: " << maxPower << endl;  
        }
    }
    if(bytesRead == -1){
        perror("smash error: getdents64 failed");
        close(fd);
        return;
    }
    if(close(fd) == -1){
        perror("smash error: close failed");
        return;
    }
    if(devnum == 0){
        cerr << "smash error: usbinfo: no USB devices found" << endl;
    }
}