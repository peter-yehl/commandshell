#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>  
#include <linux/limits.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

void shellLoop(void);
char *shellRead(void);
char **shellParse(char*);
void shellExecute(char**);
char *prompt = "308sh> ";

void shell_cd(char**);
void shell_ls(void);
void terminate_children(void);

//job struct acts as a single node for each background process
typedef struct job{ 
    char command[10]; //stores the command from instruction[0]
    int pid;          //stores the specified pid
    struct job *next; //points to the next node in the list
} job;

job *list = NULL;

void add_job(int, char*, job**);
job *find_job(int, job*);
void delete_job(job*, job**);

//Shell initiates with a prompt before cycling through read, parse, & execute
int main(int argc, char **argv){
    //assign prompt
    if(argc > 1 && strcmp(argv[1], "-p") == 0){
        if(argc > 2){
            prompt = argv[2];
        }
        else{
            fprintf(stderr, "Missing argument for -p\n");
            return 1;
        }
    }

    char *instr = (char*)malloc(sizeof(char) * 100);
    char **parsed = (char**)malloc(sizeof(char*) * 50);

    //shell loops until user prompts to exit
    do{
        printf(prompt);
        fflush(stdout);

        instr = shellRead();
        parsed = shellParse(instr);
        shellExecute(parsed);

        //checks for background processes at the end, (2.1 6C)
        //"To do this periodically, a check can be done every time the user enters a command."
        terminate_children();
        free(instr);
        free(parsed);
    } while(1);

    return 0;
}

/////////////////////////////////////////////////STEP 1/////////////////////////////////////////////////
//shellRead gathers user input
char *shellRead(void){
    //user input can be up to 99 characters
    char *content = (char*)malloc(sizeof(char) * 100);
    int c;
    int i = 0;

    while(1){
        c = getchar();

        if(c == '\0' || c == '\n' || i == 99){
            content[i] = '\0';
            break;
        }
        else{
            content[i] = c;
            i++;
        }
    }
    return content;
}

/////////////////////////////////////////////////STEP 2/////////////////////////////////////////////////
//parse through user input
char **shellParse(char* prompt){
    //if no input
    if(prompt == NULL || prompt[0] == '\0'){
        return NULL;
    }

    //holds up to 50 commands since worst case is 1 char per command
    char *command;
    char **commands = (char**)malloc(sizeof(char*) * 50);
    int i = 0;

    //parses prompt at variety of delimiters
    command = strtok(prompt, " \t\r\n\a");

    //continue creating commands until command is NULL
    while (command != NULL) {
        commands[i] = command;
        i++;
        command = strtok(NULL, " \t\r\n\a"); 
    }
    //append with NULL to correctly finalize list of commands
    commands[i] = NULL;

    return commands;
}

/////////////////////////////////////////////////STEP 3/////////////////////////////////////////////////
//execute user input commands
void shellExecute(char** instruction){
    //if no input
    if(instruction == NULL || instruction[0] == NULL){
        return;
    }
    //background process flag
    int pBack = 0;

    //find length of words in instruction
    int i = 0;
    while(instruction[i] != NULL){
        i++;
    }
    //if final word is "&", denote command as background process
    if(strcmp(instruction[i - 1], "&") == 0){
        pBack = 1;
        instruction[i - 1] = NULL;
    }

    /**
     * BUILT-IN COMMANDS
     */
    //exit shell
    if(strcmp(instruction[0], "exit") == 0){
        exit(0);
    }
    //change directory
    else if(strcmp(instruction[0], "cd") == 0){
        shell_cd(instruction);
    }
    //get current directory
    else if(strcmp(instruction[0], "pwd") == 0){
        char cwd[PATH_MAX];
        if(getcwd(cwd, sizeof(cwd)) == NULL){
            perror("Couldn't Find Current Directory");
        }
        else{
            printf("Current Directory: %s\n", getcwd(cwd, sizeof(cwd)));
        }
    }
    //get pid
    else if(strcmp(instruction[0], "pid") == 0){
        printf("Process ID: %d\n", getpid());
    }
    //get parent pid
    else if(strcmp(instruction[0], "ppid") == 0){
        printf("Parent Process ID: %d\n", getppid());
    }
    //EXTRA CREDIT - returns list of current background processes
    else if(strcmp(instruction[0], "jobs") == 0){
        printf("--START OF JOBS LIST--\n");
        job *curr = list;
        while(curr != NULL){
            printf("[%d] %s\n", curr->pid, curr->command);
            curr = curr->next;
        }
        printf("--END OF JOBS LIST--\n");
    }

    /**
     * PROGRAM COMMANDS
     */
    else{
        int status;
        pid_t pid;
        pid = fork();
        if (pid < 0){
            fprintf(stderr, "Fork Failed\n");
        }
        //if child process, begin to execute instruction
        else if (pid == 0){
            execvp(instruction[0], instruction);
            perror("Command Failed");
            kill(getpid(), SIGTERM);
        }
        //if parent process
        else
        {
            //print process id from init
            printf("[%d] %s\n", pid, instruction[0]);

            //if process isn't background, print completion id and exit status
            if (pBack == 0){
                waitpid(pid, &status, 0);
                printf("[%d] %s ", pid, instruction[0]);
                if (WIFEXITED(status)){
                    printf("Exit status %d\n", WEXITSTATUS(status));
                }
                else if (WIFSIGNALED(status)){
                    printf("Exit signal %d\n", WTERMSIG(status));
                }
            }
            else{
                add_job(pid, instruction[0], &list);
            }
        }
    }
    fflush(stdout);
}

/////////////////////////////////////////////////STEP 4/////////////////////////////////////////////////
//terminate all completed background processes from the process table
void terminate_children(){
    int status;
    pid_t pid;

    //terminates one completed child at a time
    while(1){
        pid = waitpid(-1, &status, WNOHANG);
        //finds job based on pid
        job *found = find_job(pid, list);
        if(pid > 0){
            //print the completed process id and exit status (defined in 2.1, 5C)
            if (WIFEXITED(status) && found != NULL){
                printf("[%d] %s Exit status %d\n", found->pid, found->command, WEXITSTATUS(status));
            } 
            else if(WIFSIGNALED(status) && found != NULL){
                printf("[%d] %s Exit signal %d\n", found->pid, found->command, WTERMSIG(status));
            }
            fflush(stdout);
            //delete job from list
            delete_job(found, &list);
        }
        else{
            break;
        }
    }
}

/////////////////////////////////////////////////Additional Functions/////////////////////////////////////////////////
//change directory function called from shellExecute
void shell_cd(char **instr){
    char cwd[PATH_MAX];

    //switch to home if path isn't specified
    if(instr[1] == NULL){
        chdir(getenv("HOME"));
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("Directory changed to: %s\n", cwd);
        }
    }
    //else, switch to specified path
    else{
        if(chdir(instr[1]) == 0){
            if (getcwd(cwd, sizeof(cwd)) != NULL) {
                printf("Directory changed to: %s\n", cwd);
            }
        }
        else{
            perror("Path Doesn't Exist");
        }
    }
}

//adds job and its params to linked list
void add_job(int pid, char *command, job **list){
    job *newJob = (job *)malloc(sizeof(job));
    newJob->pid = pid;
    strcpy(newJob->command, command);
    strcat(newJob->command, "\0");

    newJob->next = *list;
    *list = newJob;
}

//finds job based on pid
job *find_job(int pid, job *list){
    job *curr = list;
    //cycles through each node until matching pid is found
    while(curr != NULL){
        if(curr->pid == pid){
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

//removes job from linked list
void delete_job(job *found, job **list){
    job *curr = *list;
    job *prev = NULL;
    //cycles through each node until matching node is found
    while(curr != NULL && curr != found){
        prev = curr;
        curr = curr->next;
    }

    //deletes node from list
    if(prev != NULL){
        prev->next = curr->next;
    }
    else{
        *list = curr->next;
    }
    free(curr);
}
