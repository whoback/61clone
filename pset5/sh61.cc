#include "sh61.hh"
#include <cstring>
#include <cerrno>
#include <vector>
#include <sys/stat.h>
#include <sys/wait.h>
#include <list>


// struct command
//    Data structure describing a command. Add your own stuff.

struct command {
    std::vector<std::string> args;
    pid_t pid;      // process ID running this command, -1 if none
    command* next;
    command* command_sibling;
    int op; // Operator following this command. Equals one of the TOKEN_ constants;
            // always TOKEN_SEQUENCE or TOKEN_BACKGROUND for last in list.
            // Initialize `next` and `op` when a `command` is allocated.
    bool is_background;
    command();
    ~command();

    pid_t make_child(pid_t pgid);
};

// command::command()
//    This constructor function initializes a `command` structure. You may
//    add stuff to it as you grow the command structure.

command::command() {
    this->pid = -1;
    next = nullptr;
    command_sibling = nullptr;
    op = TYPE_SEQUENCE;
    is_background = false;
}


// command::~command()
//    This destructor function is called to delete a command.

command::~command() {
}

struct pipeline
{
    command *command_child = nullptr;
    pipeline *pipeline_sibling = nullptr;
    bool is_or = false;
};
struct conditional
{
    pipeline *pipeline_child = nullptr;
    conditional *conditional_sibling = nullptr;
    bool is_background = false;
};
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

pid_t command::make_child(pid_t pgid) {
    assert(this->args.size() > 0);
    (void) pgid; // You won’t need `pgid` until part 8.
    // Your code here!
    pid_t p;
    std::vector<char *> argc;
    int r;
    switch(p = fork())
    {
        //error
        case -1:
            fprintf(stderr, "fork() failed.\n");
            _exit(1);
        //child process
        case 0:
            p = getpid();
            this->pid = p;
            for (auto const &a : args)
            {
                argc.emplace_back(const_cast<char *>(a.c_str()));
            }
            // NULL terminate
            argc.push_back(nullptr);

            r = execvp(argc[0], argc.data());

            if (r == -1)
            {
                fprintf(stderr, "execvp() failed.\n");
                _exit(1);
            }
            _exit(0);
        //parent process
        default:
            int status;
            (void)waitpid(pid, &status, 0);
           
     
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

void run(command* c) {
    int status;
    if(!c->args.empty())
    {
        c->make_child(0);
    }
    if(c->is_background==true)
    {
        c->make_child(c->pid);
    }

    waitpid(c->pid, &status, 0);
    if(!WIFEXITED(status))
    {
        perror("Error in run");
    }
}


// parse_line(s)
//    Parse the command list in `s` and return it. Returns `nullptr` if
//    `s` is empty (only spaces). You’ll extend it to handle more token
//    types.

command* parse_line(const char* s) {
    int type;
    std::string token;
    // Your code here!
    if ((s != NULL) && (s[0] == '\0'))
    {
        return nullptr;
    }
        // build the command
        // (The handout code treats every token as a normal command word.
        // You'll add code to handle operators.)
        command *c = nullptr;
        c = new command;
        
        while ((s = parse_shell_token(s, &type, &token)) != nullptr)
        {

                if (type == TYPE_NORMAL)
                {
                    c->args.push_back(token);
                }
                if (type == TYPE_BACKGROUND)
                {
                    c->is_background = true;
                }        
                if(type == TYPE_SEQUENCE)
                {
                    c->next = new command;
                    c = c->next;
                }
                if(type == TYPE_AND)
                {
                    c->next = new command;
                    c = c->next;
                }
        
                       
        }
    return c;
}

int main(int argc, char* argv[]) {
    FILE* command_file = stdin;
    bool quiet = false;

    // Check for '-q' option: be quiet (print no prompts)
    if (argc > 1 && strcmp(argv[1], "-q") == 0) {
        quiet = true;
        --argc, ++argv;
    }

    // Check for filename option: read commands from file
    if (argc > 1) {
        command_file = fopen(argv[1], "rb");
        if (!command_file) {
            perror(argv[1]);
            exit(1);
        }
    }

    // - Put the shell into the foreground
    // - Ignore the SIGTTOU signal, which is sent when the shell is put back
    //   into the foreground
    claim_foreground(0);
    set_signal_handler(SIGTTOU, SIG_IGN);

    char buf[BUFSIZ];
    int bufpos = 0;
    bool needprompt = true;

    while (!feof(command_file)) {
        // Print the prompt at the beginning of the line
        if (needprompt && !quiet) {
            printf("sh61[%d]$ ", getpid());
            fflush(stdout);
            needprompt = false;
        }

        // Read a string, checking for error or EOF
        if (fgets(&buf[bufpos], BUFSIZ - bufpos, command_file) == nullptr) {
            if (ferror(command_file) && errno == EINTR) {
                // ignore EINTR errors
                clearerr(command_file);
                buf[bufpos] = 0;
            } else {
                if (ferror(command_file)) {
                    perror("sh61");
                }
                break;
            }
        }

        // If a complete command line has been provided, run it
        bufpos = strlen(buf);
        if (bufpos == BUFSIZ - 1 || (bufpos > 0 && buf[bufpos - 1] == '\n')) {
            if (command* c = parse_line(buf)) {
                run(c);
                delete c;
            }
            bufpos = 0;
            needprompt = 1;
        }

        // Handle zombie processes and/or interrupt requests
        // Your code here!
    }

    return 0;
}
