// Author: Hoang Ho
// Date: 5/25/2023
// Description: This project is a portfolio assignment of Small Shell in Operating Systems class, which replicates subset of
//              features of a bash shell.
// Functionalities: 
// - Provide a prompt for running commands
// - Handle blank lines and comments, which begins with # character
// - Provide expansion for the variable $$
// - Execute commands of "exit", "cd", "status" via code.
// - Execute other commands by creating new processes from the "exec" family of functions
// - Support input/output redirection
// - Support running commands in foreground and background processes
// - Implement custom handlers for 2 signals of SIGINT and SIGTSTP.
#include <sys/types.h> // pid_t
#include <unistd.h> // fork
#include <string.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>

// DollarReplace function => Replace $$ => random pid
void dollarReplace(char *source, char *string, int pid) {
    char pid_str[20];
    sprintf(pid_str, "%d", pid);

    char *p = strstr(source, string);
    do {
        if (p) {
            char buf[1024];
            memset(buf, '\0', strlen(buf));

            if (source == p) {
                strcpy(buf, pid_str);
                strcat(buf, p + strlen(string));
            } else {
                strncpy(buf, source, strlen(source) - strlen(p));               
                strcat(buf, pid_str);
                strcat(buf, p + strlen(string));
            }
            memset(source, '\0', strlen(source));
            strcpy(source, buf);
        }
    } while (p && (p = strstr(source, string)));
}

// cdCase function, => Taking the cd command
// I should not have this function at all...
// but I did this in the beginning and it was way bigger than what it was originally so why not keep it :)
int cdCase(char* userInput, int argc, char **argv) {
    char* token = strtok(userInput, " "); // cd command token
    if (argc > 1) {
        chdir(argv[1]);
    } else {
        chdir(getenv("HOME"));
    }
}   

// parseAndSpot function: parse commands, arguments, infile name, outfile name, background state.
// Forgot to make the case to truncate \n. But i was using it for the sake of background spotting 
// So... there's a lot of token truncate instead :)
void parseAndSpot(char* userInput, char* infile, char* outfile, int* background, int* argc, char** argv) {
    *argc = 0;
    *background = 0;
    char* token = strtok(userInput, " ");               // command token.
    if (strcmp(token + strlen(token) - 1, "\n") == 0) { // throwing command token to argv[0], making sure it doesn't have \n.
        token[strlen(token) - 1] = '\0';
        argv[*argc] = token;                           
        *argc = *argc + 1;
    } else {
        argv[*argc] = token;                                
        *argc = *argc + 1;
    }

    token = strtok(NULL, " ");                          // Next token proceed.
        while (token != NULL) {
            if (strcmp(token, "<") == 0) {              // "<" symbol spotting
                token = strtok(NULL, " ");
                if (strcmp(token + strlen(token) - 1, "\n") == 0) {
                    token[strlen(token) - 1] = '\0';
                }
                strcpy(infile, token);
            } 
            else if (strcmp(token, ">") == 0) {       // ">" symbol spotting
                token = strtok(NULL, " ");
                if (strcmp(token + strlen(token) - 1, "\n") == 0) {
                    token[strlen(token) - 1] = '\0';
                }
                strcpy(outfile, token);
            } 
            else if (strcmp(token, "&\n") == 0) {                                       // Since we know & would always be at the end, so next to it would be \n
                *background = 1;
            }
            else {                                                                      // If not, it's an argument.
                if (strcmp(token + strlen(token) - 1, "\n") != 0) {                     // Case 1: there's no \n in the argument ending.
                    argv[*argc] = token;
                } 
                else {                                                                  // Case 2: There's \n in the argument ending.
                    token[strlen(token) - 1] = '\0';
                    argv[*argc] = token;
                }
                *argc = *argc + 1;
            }
            token = strtok(NULL, " ");                  // Next token proceed.
        }
}

int toggleBackground = 1;           // Global variable for the sake of toggling background mode.

// SIGTSTP handling function: Toggles the background mode on or off.
void handle_sigtstp(int sig) {
    char* message = "Caught sigtstp. Toggling background/Foreground...\n";
    write(STDOUT_FILENO, message, strlen(message));
    fflush(stdout);
    if (toggleBackground == 1) {
        toggleBackground = 0;
    } else {
        toggleBackground = 1;
    }
}

// Sigint foreground function: terminates the process instantly.
void handle_sigint_fg(int sig) {
    exit(sig);
}

// otherCommands function: Handling EVERY single non-builtin commands.

void otherCommands(char* userInput, char** argv, int *pid, char* infile, char* outfile, int* background, int* childStatus) {
    int input;
    int output;
    *pid = fork();

    switch(*pid) {
        case -1: 
            perror("Fork failed...");
            exit(1);
            break;
        case 0: {
            struct sigaction sigintChildAction = {0};
            struct sigaction sigtstpChildAction = {0};
            
            if (*background == 1) {                                     // Background process
                sigintChildAction.sa_handler = SIG_IGN;                      // => Child background ignores sigint
                sigtstpChildAction.sa_handler = SIG_IGN;                     // => Child background ignores sigtstp

            } else {                                                    // Foreground process
                sigintChildAction.sa_handler = &handle_sigint_fg;           // => Child foreground terminates itself
                sigtstpChildAction.sa_handler = SIG_IGN;                     // => Child foreground ignores sigtstp
            }
            sigaction(SIGINT, &sigintChildAction, NULL);
            sigaction(SIGTSTP, &sigtstpChildAction, NULL);

            if (strcmp(infile, "") != 0) {                                      // Infile opening and redirection
                input = open(infile, O_RDONLY, 0644);
                if (input == -1) {
                    perror("Unable to open input file.\n");
                    fflush(stdout);
                    exit(1);
                }
                dup2(input, 0);
            } 
            if (strcmp(outfile, "") != 0) {                                     // Outfile opening and redirection
                output = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (output == -1) {
                    perror("Unable to open output file.\n");
                    exit(1);
                } 
                dup2(output, 1);
            }

            if (execvp(argv[0], argv) < 0) {
                perror(":Unable to execute command. \n");
                fflush(stdout);
                exit(2);
            }
        }
            break;

        default:
            if (*background == 1 && toggleBackground == 1) {                   // Background case
                pid_t save_pid = waitpid(*pid, childStatus, WNOHANG);
                printf("Background PID: %d\n", *pid);
                fflush(stdout);
            } else {                                                            // Foreground case
                pid_t save_pid = waitpid(*pid, childStatus, 0); 
            }

            while ((*pid = waitpid(-1, childStatus, WNOHANG)) > 0) {                 // Background process termination
                printf("Child %d has been terminated.\n", *pid);
                if (WTERMSIG(childStatus)) {
                        printf("Exit signal: %d\n", (WTERMSIG(*childStatus)));
                        fflush(stdout);
                    } else {
                        printf("Exit value status: %d\n", (WIFEXITED(*childStatus)));
                        fflush(stdout);
                    }
            }
            break;
    }
}

// Main function :)
int main() {
    pid_t spawnpid = -5;
    int pid = getpid();
    int childStatus = 0;
    bool stillRunning = true;

    char userInput[2049] = {0};
    char *token;
    int argc = 0;
    int background = 0;

    struct sigaction sigintAction = {0};                 // The parent struct: parent process ignores sigint.
    sigintAction.sa_handler = SIG_IGN;
    sigintAction.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sigintAction, NULL);

    struct sigaction sigtstpAction = {0};               // The parent struct: parent toggles background/foreground for sigtstp
    sigtstpAction.sa_handler = &handle_sigtstp;
    sigtstpAction.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &sigtstpAction, NULL);

    while (stillRunning == true) {
                char input_file[256] = {0};
                char output_file[256] = {0};
                printf(":");                                                    // Prints colon every time.
                fflush(stdout);
                char* argv[512] = {0};
                fgets(userInput, 2048, stdin);                                  // Takes user input.
                dollarReplace(userInput, "$$", pid);                            // "$$" => PID function.
                parseAndSpot(userInput, input_file, output_file, &background, &argc, argv); 
                if (strcmp(userInput, "") == 0) {                               // Empty case
                    continue;
                }
                else if (strncmp(userInput, "#", strlen("#")) == 0) {          // Comment case
                    continue;
                } 
                else if (strcmp(userInput, "exit") == 0) {                     // Exit case
                    stillRunning = false;
                }

                else if (strncmp(userInput, "cd", strlen("cd")) == 0) {        // cd case
                    cdCase(userInput, argc, argv);
                }
                
                else if (strcmp(userInput, "status") == 0) {                   // Status case
                    if (WTERMSIG(childStatus)) {
                        printf("Exit signal: %d\n", (WTERMSIG(childStatus)));
                    } else {
                        printf("Exit value status: %d\n", (WIFEXITED(childStatus)));
                    }
                } else {
                    otherCommands(userInput, argv, &spawnpid, input_file, output_file, &background, &childStatus);
                }
    }
    return 0;
}

/* Sample execution: 
$ smallsh
: /bin/timedatectl
               Local time: Thu 2023-05-25 15:26:30 PDT
           Universal time: Thu 2023-05-25 22:26:30 UTC
                 RTC time: Sun 2023-05-25 22:26:31
                Time zone: America/Los_Angeles (PDT, -0700)
System clock synchronized: yes
              NTP service: active
          RTC in local TZ: no
: ls > junk
: status
exit value 0
: cat junk
junk
smallsh
smallsh.c
: wc < junk > junk2
: wc < junk
       3       3      23
: test -f badfile
: status
exit value 1
: wc < badfile
bash: badfile: No such file or directory
: status
exit value 1
: badfile
bash: badfile: command not found
: sleep 5
^Cterminated by signal 2
: status &
terminated by signal 2
: sleep 15 &
background pid is 4923
: ps
  PID TTY          TIME CMD
 4923 pts/0    00:00:00 sleep
 4564 pts/0    00:00:03 bash
 4867 pts/0    00:01:32 smallsh
 4927 pts/0    00:00:00 ps
:
: # that was a blank command line, this is a comment line
:
background pid 4923 is done: exit value 0
: # the background sleep finally finished
: sleep 30 &
background pid is 4941
: kill -15 4941
background pid 4941 is done: terminated by signal 15
: pwd
/nfs/stak/users/hohoan/CS344/prog3
: cd
: pwd
/nfs/stak/users/hohoan
: cd CS344
: pwd
/nfs/stak/users/hohoan/CS344
: echo 4867
4867
: echo $$
4867
: ^C^Z
Entering foreground-only mode (& is now ignored)
: date
 Mon Jan  2 11:24:33 PST 2017
: sleep 5 &
: date
 Mon Jan  2 11:24:38 PST 2017
: ^Z
Exiting foreground-only mode
: date
 Mon Jan  2 11:24:39 PST 2017
: sleep 5 &
background pid is 4963
: date
 Mon Jan 2 11:24:39 PST 2017
: exit
$*/