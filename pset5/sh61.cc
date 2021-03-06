#include "sh61.hh"
#include <cstring>
#include <cerrno>
#include <vector>
#include <sys/stat.h>
#include <sys/wait.h>

volatile sig_atomic_t interrupt = 0;

// struct command
//    Data structure describing a command. Add your own stuff.

struct command
{
    std::vector<std::string> args;
    pid_t pid; // process ID running this command, -1 if none
    pid_t pgid;
    int op;
    int cond;
    int pipe_read_end;
    int pipe_write_end;
    command *next;
    bool is_pipe;
    bool pipe_start;
    std::string redir_type;
    std::string infile;
    std::string outfile;
    std::string errfile;
    command();
    ~command();

    pid_t make_child(pid_t pgid);
};

// command::command()
//    This constructor function initializes a `command` structure. You may
//    add stuff to it as you grow the command structure.

command::command()
{
    this->pid = -1;
    this->pgid = -1;
    this->op = TYPE_SEQUENCE;
    this->next = nullptr;
    this->cond = -1;
    this->is_pipe = false;
    this->pipe_read_end = 0;
    this->pipe_write_end = 1;
    this->pipe_start = false;
}

// command::~command()
//    This destructor function is called to delete a command.

command::~command()
{
    // delete next;
}

// COMMAND EXECUTION

// command::make_child(pgid)
//    Create a single child process running the command in `this`.
//    Sets `this->pid` to the pid of the child process and returns `this->pid`.
//
//    PART 1: Fork a child process and run the command using `execvp`.
//       This will require creating an array of `char*` arguments using
//       `this->args[N].c_str()`.
//    PART 5: Set up a pipeline if appropriate. This may require creating a
//       new pipe (`pipe` system call), and/or replacing the child process's
//       standard input/output with parts of the pipe (`dup2` and `close`).
//       Draw pictures!
//    PART 7: Handle redirections.
//    PART 8: The child process should be in the process group `pgid`, or
//       its own process group (if `pgid == 0`). To avoid race conditions,
//       this will require TWO calls to `setpgid`.

pid_t command::make_child(pid_t pgid)
{
    // assert(this->args.size() > 0);
    (void)pgid; // You won’t need `pgid` until part 8.
    // Your code here!
    pid_t p;
    std::vector<char *> argc;
    int r;
    // create pipes
    int inpfd[2] = {-1, -1};
    if(is_pipe)
    {
        r = pipe(inpfd);
        if (r < 0)
        {
            fprintf(stderr, "Error: pipe() in make_child failed to execute\n");
            _exit(1);
        }
        this->pipe_write_end = inpfd[1];
        if(this->next)
        {
            this->next->pipe_read_end = inpfd[0];
            this->next->pgid = pgid;
        }
    }

    //CD
    if(args[0] == "cd")
    {
        r = chdir(args[1].c_str());
        if (r != 0)
        {
            chdir("/");
        }
    }
    p = fork();
    if (p == -1)
    {
        // fprintf(stderr, "Error: fork() in make_child failed to execute\n");
        _exit(1);
    }
    // in child
    else if (p == 0)
    {
        if(this->pipe_start)
        {
            pgid = getpid();
            this->next->pgid = pgid;
        }
        setpgid(getpid(), pgid);
        dup2(this->pipe_read_end, 0);
        dup2(this->pipe_write_end, 1);

        if (this->pipe_read_end != 0)
        {
            close(this->pipe_read_end);
        }
        if (this->pipe_write_end != 1)
        {
            close(this->pipe_write_end);
        }
        if (this->is_pipe)
        {
            close(inpfd[0]);
        }
        if(this->redir_type == "<")
        {
            int fid = open(this->infile.c_str(), O_RDONLY | O_CLOEXEC);
            if (fid == -1)
            {
                fprintf(stderr, "No such file or directory\n");
                _exit(1);
            }
            dup2(fid, 0);
        }
        if(this->redir_type == ">")
        {
            int fid = open(this->outfile.c_str(), O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU);
            if (fid == -1)
            {
                fprintf(stderr, "No such file or directory\n");
                _exit(1);
            }
            dup2(fid, 1);
        }
        if(this->redir_type == "2>")
        {
            int fid = open(this->errfile.c_str(), O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU);
            if (fid == -1)
            {
                fprintf(stderr, "No such file or directory\n");
                _exit(1);
            }
            dup2(fid, 2);
        }
        for (auto const &a : args)
        {
            argc.emplace_back(const_cast<char *>(a.c_str()));
        }
        // NULL terminate
        argc.push_back(nullptr);

        r = execvp(argc[0], argc.data());
        if (r == -1)
        {
            // fprintf(stderr, "execvp() failed.\n");
            // perror("exec failed:");
            _exit(1);
        }
        _exit(0);
    }
    //in parent
    else
    {
        if (this->pipe_read_end != 0)
        {
            close(this->pipe_read_end);
        }
        if (this->pipe_write_end != 1)
        {
            close(this->pipe_write_end);
        }
        this->pid = p;
    }
    
    return this->pid;
}

// run(c)
//    Run the command *list* starting at `c`. Initially this just calls
//    `make_child` and `waitpid`; you’ll extend it to handle command lists,
//    conditionals, and pipelines.
//
//    PART 1: Start the single command `c` with `c->make_child(0)`,
//        and wait for it to finish using `waitpid`.
//    The remaining parts may require that you change `struct command`
//    (e.g., to track whether a command is in the background)
//    and write code in `run` (or in helper functions).
//    PART 2: Treat background commands differently.
//    PART 3: Introduce a loop to run all commands in the list.
//    PART 4: Change the loop to handle conditionals.
//    PART 5: Change the loop to handle pipelines. Start all processes in
//       the pipeline in parallel. The status of a pipeline is the status of
//       its LAST command.
//    PART 8: - Choose a process group for each pipeline and pass it to
//         `make_child`.
//       - Call `claim_foreground(pgid)` before waiting for the pipeline.
//       - Call `claim_foreground(0)` once the pipeline is complete.

void run(command *c)
{
    int status;
    int prev_type = CONDMAGIC;
    int prev_status = CONDMAGIC;
    pid_t exited_pid;
    pid_t p;
    //bool ret = chain_in_background(c);
    //do we have a bg chain?
    command *bghead = nullptr;
    command *restoflist = nullptr;
    if (chain_in_background(c))
    {
        bghead = c;
        while (c != nullptr)
        {
            if (c->op == TYPE_BACKGROUND)
            {
                
                c->op = TYPE_SEQUENCE;
                restoflist = c->next;
                c->next = nullptr;
                break;
            }
            c = c->next;
        }
        pid_t p = fork();
        if (p == -1)
        {
            _exit(1);
        }
        if (p == 0)
        {
            run(bghead);
            delete(bghead);
            _exit(0);
        }

        run(restoflist);
        delete(restoflist);
        _exit(0);
        // return;
    }
    if (c->args.empty())
    {
        c = c->next;
    }
    while (c != nullptr)
    {
        
        if(prev_type == TYPE_SEQUENCE || prev_type == CONDMAGIC)
        {
            //just run a command
            p = c->make_child(0);
            exited_pid = waitpid(p, &status, 0);
            if (WIFEXITED(status) !=0)
            {
                prev_status = WEXITSTATUS(status);
            }
            prev_type = c->op;
        }
        else if(prev_type == TYPE_AND)
        {
            //this means that this command is the first to run
           if(prev_status == 0)
           {
               p = c->make_child(0);
               exited_pid = waitpid(p, &status, 0);
               if (WIFEXITED(status) != 0)
               {
                   prev_status = WEXITSTATUS(status);
               }
               prev_type = c->op;
           }
           
           prev_type = c->op;
        }
        else if(prev_type == TYPE_OR)
        {
            if (prev_status != 0)
            {
                p = c->make_child(0);
                exited_pid = waitpid(p, &status, 0);
                if (WIFEXITED(status) != 0)
                {
                    prev_status = WEXITSTATUS(status);
                }
                prev_type = c->op;
            }
            if(prev_status != 0)
            prev_type = c->op;
        }
        
        //check if the previous type was a conditional
        
        prev_type = c->op;
        
        //move forward to next command
        c = c->next;
    }
}

// parse_line(s)
//    Parse the command list in `s` and return it. Returns `nullptr` if
//    `s` is empty (only spaces). You’ll extend it to handle more token
//    types.

command *parse_line(const char *s)
{
    int type;
    std::string token;
    // Your code here!
    // build the command
    // (The handout code treats every token as a normal command word.
    // You'll add code to handle operators.)
    command *c = nullptr;
    command *head = nullptr;
    while ((s = parse_shell_token(s, &type, &token)) != nullptr)
    {
        if (!c)
        {
            c = new command;
            if (!head)
            {
                head = c;
            }
        }
        if (type == TYPE_NORMAL)
        {
            c->args.push_back(token);
        }

        if (type == TYPE_BACKGROUND)
        {
            c->op = type;
            c->next = new command;
            c = c->next;
        }
        if (type == TYPE_SEQUENCE && token != "")
        {
            c->op = type;
            c->next = new command;
            c = c->next;
        }
        if (type == TYPE_AND)
        {
            c->op = type;
            c->cond = 1;
            c->next = new command;
            c = c->next;
        }
        if (type == TYPE_OR)
        {
            c->op = type;
            c->cond = 1;
            c->next = new command;
            c = c->next;
        }
        if(type == TYPE_PIPE)
        {
            if(!c->is_pipe)
            {
                c->pipe_start = true;
            }
            c->is_pipe = true;
            c->next = new command;
            c = c->next;
        }
        if(type == TYPE_REDIRECTION)
        {
            c->redir_type = token;
            std::string redir_type = token;
            s = parse_shell_token(s, &type, &token);
            if (redir_type == "<")
            {
                c->infile = token;
            }
            if (redir_type == ">")
            {
                c->outfile = token;
            }
            if (redir_type == "2>")
            {
                c->errfile = token;
            }
        }
    }
    c->next = nullptr;
    return head;
}
bool chain_in_background(command *c)
{
    //loop through list
    while (c->op != TYPE_SEQUENCE && c->op != TYPE_BACKGROUND)
    {
        // printf("type: %d\n", c->op);
        c = c->next;
    }
    //if true this in the background
    return c->op == TYPE_BACKGROUND;
}

void int_handler(int i)
{
    interrupt = 1;
}
int main(int argc, char *argv[])
{
    FILE *command_file = stdin;
    bool quiet = false;

    // Check for '-q' option: be quiet (print no prompts)
    if (argc > 1 && strcmp(argv[1], "-q") == 0)
    {
        quiet = true;
        --argc, ++argv;
    }

    // Check for filename option: read commands from file
    if (argc > 1)
    {
        command_file = fopen(argv[1], "rb");
        if (!command_file)
        {
            perror(argv[1]);
            exit(1);
        }
    }

    // - Put the shell into the foreground
    // - Ignore the SIGTTOU signal, which is sent when the shell is put back
    //   into the foreground
    claim_foreground(0);
    set_signal_handler(SIGTTOU, SIG_IGN);
    set_signal_handler(SIGINT, int_handler);

    char buf[BUFSIZ];
    int bufpos = 0;
    bool needprompt = true;

    while (!feof(command_file))
    {
        // Print the prompt at the beginning of the line
        if (needprompt && !quiet)
        {
            printf("sh61[%d]$ ", getpid());
            fflush(stdout);
            needprompt = false;
        }

        // Read a string, checking for error or EOF
        if (fgets(&buf[bufpos], BUFSIZ - bufpos, command_file) == nullptr)
        {
            if (ferror(command_file) && errno == EINTR)
            {
                // ignore EINTR errors
                clearerr(command_file);
                buf[bufpos] = 0;
            }
            else
            {
                if (ferror(command_file))
                {
                    perror("sh61");
                }
                break;
            }
        }

        // If a complete command line has been provided, run it
        bufpos = strlen(buf);
        if (bufpos == BUFSIZ - 1 || (bufpos > 0 && buf[bufpos - 1] == '\n'))
        {
            if (command *c = parse_line(buf))
            {
                run(c);
                delete c;
            }
            bufpos = 0;
            needprompt = 1;
        }

        // Handle zombie processes and/or interrupt requests
        // Your code here!
        int wstatus;
        while (waitpid(-1, &wstatus, WNOHANG) > 0)
        {
        }
        if (interrupt == 1)
        {
            kill(getpid(), SIGINT);
            needprompt = true;
            interrupt = 0;
            continue;
        }
    }

    return 0;
}