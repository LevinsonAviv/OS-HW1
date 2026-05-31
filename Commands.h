// Ver: 04-11-2025
#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <list>
#include <string>

#define COMMAND_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

class Command {
private:
    const char *cmd_line;

public:
    Command(const char *cmd_line);

    virtual ~Command() = default;

    virtual void execute() = 0;

    const char* getCmdLine() const;

    std::string getAliasCmdLine() const;

    int getAliasArgs(char **args) const;
    
    virtual bool isBackgroundable() const;
    
};

class BuiltInCommand : public Command {
public:
    BuiltInCommand(const char *cmd_line);

    virtual ~BuiltInCommand() = default;
};

class ExternalCommand : public Command {
public:
    ExternalCommand(const char *cmd_line);

    virtual ~ExternalCommand() = default;

    bool isBackgroundable() const override;
};

class SimpleExternalCommand : public ExternalCommand {
public:
    SimpleExternalCommand(const char *cmd_line);

    virtual ~SimpleExternalCommand() = default;

    void execute() override;
};

class ComplexExternalCommand : public ExternalCommand {
public:
    ComplexExternalCommand(const char *cmd_line);

    virtual ~ComplexExternalCommand() = default;

    void execute() override;
};


class RedirectionCommand : public Command {
public:
    explicit RedirectionCommand(const char *cmd_line);

    virtual ~RedirectionCommand() = default;

    void execute() override;
};

class PipeCommand : public Command {
public:
    PipeCommand(const char *cmd_line);

    virtual ~PipeCommand() = default;

    void execute() override;
};

class DiskUsageCommand : public Command {
private:
    static int total_du;
    static int nftwCallback(const char *fpath, const struct stat *statsBuffer, int additionalInfo, struct FTW *ftw);
public:
    DiskUsageCommand(const char *cmd_line);

    virtual ~DiskUsageCommand() = default;

    void execute() override;

    bool isBackgroundable() const override;
};

class WhoAmICommand : public Command {
public:
    WhoAmICommand(const char *cmd_line);

    virtual ~WhoAmICommand() = default;

    void execute() override;

    bool isBackgroundable() const override;
};

class USBInfoCommand : public Command {
private:
    struct USBEntry {
        int devnum;
        std::string vendorID;
        std::string productID;
        std::string manufacturer;
        std::string productName;
        std::string maxPower;
    };

public:
    USBInfoCommand(const char *cmd_line);

    virtual ~USBInfoCommand() = default;

    void execute() override;

    bool isBackgroundable() const override;
};

class ChangePromptCommand : public BuiltInCommand {
public:
    ChangePromptCommand(const char *cmd_line);

    virtual ~ChangePromptCommand() = default;

    void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {
public:
    ChangeDirCommand(const char *cmd_line);

    virtual ~ChangeDirCommand() = default;

    void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
public:
    GetCurrDirCommand(const char *cmd_line);

    virtual ~GetCurrDirCommand() = default;

    void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
public:
    ShowPidCommand(const char *cmd_line);

    virtual ~ShowPidCommand() = default;

    void execute() override;
};

class JobsList;

class QuitCommand : public BuiltInCommand {
public:
    QuitCommand(const char *cmd_line);

    virtual ~QuitCommand() = default;

    void execute() override;
};

class JobsList {
public:
    class JobEntry {
        private:
            int jid;
            pid_t jobPid;
            std::string cmdLine;

        public:

            JobEntry(int jid, pid_t jobPid, const std::string& cmdLine);
            int getJid() const;
            pid_t getJobPid() const;
            std::string getCmdLine() const;
    };

    JobsList() = default;

    ~JobsList() = default;

    int getSize() const;

    void addJob(const Command& cmd, pid_t jobPid);

    void printJobsListByJid();

    void printJobsListByPid();

    void killAllJobs();

    void removeFinishedJobs();

    JobEntry *getJobById(int jobId);

    void removeJobById(int jobId);

    JobEntry *getLastJob();

private:
    std::list<JobEntry> list;

};

class AliasesList{
private:
    struct AliasEntry{
        std::string alias;
        std::string command;
    };

    std::vector<AliasEntry> aliases;

    static const std::vector<std::string> reservedKeywords;

public:
    AliasesList() = default;
    ~AliasesList() {}; // change
    std::string getCommandFromAlias(const std::string& alias) const;
    void addAlias(const std::string& alias, const std::string& command);
    bool removeAlias(const std::string& alias);
    void printAliases() const;
    bool isValidAlias(const std::string& alias) const;
};

class JobsCommand : public BuiltInCommand {
public:
    JobsCommand(const char *cmd_line);

    virtual ~JobsCommand() = default;

    void execute() override;
};

class KillCommand : public BuiltInCommand {
public:
    KillCommand(const char *cmd_line);

    virtual ~KillCommand() = default;

    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
public:
    ForegroundCommand(const char *cmd_line);

    virtual ~ForegroundCommand() {
    }

    void execute() override;
};

class AliasCommand : public BuiltInCommand {
public:
    AliasCommand(const char *cmd_line);

    virtual ~AliasCommand() = default;

    void execute() override;
};

class UnAliasCommand : public BuiltInCommand {
public:
    UnAliasCommand(const char *cmd_line);

    virtual ~UnAliasCommand() = default;

    void execute() override;
};

class UnSetEnvCommand : public BuiltInCommand {
public:
    UnSetEnvCommand(const char *cmd_line);

    virtual ~UnSetEnvCommand() = default;

    void execute() override;
};

class SysInfoCommand : public BuiltInCommand {
public:
    SysInfoCommand(const char *cmd_line);

    virtual ~SysInfoCommand() = default;

    void execute() override;
};

class SmallShell {
private:
    std::string prompt;
    std::string plastPwd;
    JobsList jobs;
    AliasesList aliases;
    pid_t foregroundPid;

    SmallShell();

public:
    Command *CreateCommand(const char *cmd_line);

    SmallShell(SmallShell const &) = delete; // disable copy ctor
    void operator=(SmallShell const &) = delete; // disable = operator
    static SmallShell &getInstance() // make SmallShell singleton
    {
        static SmallShell instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }

    ~SmallShell() = default;

    void executeCommand(const char *cmd_line);

    std::string getPrompt() const;
    void setPrompt(const std::string& prompt);
    std::string getPlastPwd() const;
    void setPlastPwd(const std::string& plastPwd);
    pid_t getForegroundPid() const;
    void setForegroundPid(pid_t foregroundPid);
    JobsList& getJobsList();
    AliasesList& getAliasesList();
};

struct linux_dirent64 {
               ino64_t        d_ino;    /* 64-bit inode number */
               off64_t        d_off;    /* Not an offset; see getdents() */
               unsigned short d_reclen; /* Size of this dirent */
               unsigned char  d_type;   /* File type */
               char           d_name[]; /* Filename (null-terminated) */
           };

#endif //SMASH_COMMAND_H_