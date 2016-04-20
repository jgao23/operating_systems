#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include <sys/stat.h>
#include <fcntl.h>

#include "tokenizer.h"

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor for the shell input */
int shell_terminal;

/* Terminal mode settings for the shell */
struct termios shell_tmodes;

/* Process group id for the shell */
pid_t shell_pgid;

int cmd_exit(struct tokens *tokens);
int cmd_help(struct tokens *tokens);
int cmd_pwd(struct tokens *tokens);
int cmd_cd(struct tokens *tokens);
int cmd_wait(struct tokens *tokens);

/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t(struct tokens *tokens);

/* Built-in command struct and lookup table */
typedef struct fun_desc {
  cmd_fun_t *fun;
  char *cmd;
  char *doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
  {cmd_help, "?", "show this help menu"},
  {cmd_exit, "exit", "exit the command shell"},
  {cmd_pwd, "pwd", "prints the working director" },
  {cmd_cd, "cd", "changes the current working directory to the arg directory"},
  {cmd_wait, "wait", "waits until all background jobs have terminated before returning to the prompt"} 
};

/* Prints a helpful description for the given command */
int cmd_help(struct tokens *tokens) {
  for (int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
  return 1;
}

/* Exits this shell */
int cmd_exit(struct tokens *tokens) {
  exit(0);
}

int cmd_pwd(struct tokens *tokens) {
  char cwd[1024];
  if (getcwd(cwd, sizeof(cwd)) != NULL) {
    fprintf(stdout, "%s\n", cwd);
  }
  return 0;
}

int cmd_cd(struct tokens *tokens) {
  char path[1024];
  if (getcwd(path, sizeof(path)) != NULL) {
    strcpy(path, tokens_get_token(tokens, 1));
  }
  return chdir(path);
}

int cmd_wait(struct tokens *tokens) {
  pid_t waiting = 0;
  while (waiting != -1) {
    waiting = waitpid(-1, NULL, 0);
  }
  return 0; 
}


/* Looks up the built-in command, if it exists. */
int lookup(char cmd[]) {
  for (int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0))
      return i;
  return -1;
}


/* Intialization procedures for this shell */
void init_shell() {
  /* Our shell is connected to standard input. */
  shell_terminal = STDIN_FILENO;

  /* Check if we are running interactively */
  shell_is_interactive = isatty(shell_terminal);

  if (shell_is_interactive) {
    /* If the shell is not currently in the foreground, we must pause the shell until it becomes a
     * foreground process. We use SIGTTIN to pause the shell. When the shell gets moved to the
     * foreground, we'll receive a SIGCONT. */
    while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
      kill(-shell_pgid, SIGTTIN);

    /* Saves the shell's process id */
    shell_pgid = getpid();

    /* Take control of the terminal */
    tcsetpgrp(shell_terminal, shell_pgid);

    /* Save the current termios to a variable, so it can be restored later. */
    tcgetattr(shell_terminal, &shell_tmodes);
  }
}

void signal_handler(int sig) {
  pid_t current_pid = getpid(); 
  if (current_pid == shell_pgid) {
    signal(SIGINT, SIG_IGN);
  } else {
    signal(SIGINT, SIG_DFL);
  }
}


// void signal_handler_child() {
//   wait(&status);
// }

int main(int argc, char *argv[]) {
  init_shell();

  static char line[4096];
  int line_num = 0;

  /* Please only print shell prompts when standard input is not a tty */
  if (shell_is_interactive)
    fprintf(stdout, "%d: ", line_num);

  while (fgets(line, 4096, stdin)) {
    // Interrupt SIGINT
    signal(SIGINT, signal_handler);
    
    /* Split our line into words. */
    struct tokens *tokens = tokenize(line);

    /* Find which built-in function to run. */
    int fundex = lookup(tokens_get_token(tokens, 0));



    if (fundex >= 0) {
      cmd_table[fundex].fun(tokens);
    } else {
      /* REPLACE this to run commands as programs. */
      
      // Getting command and number of arguments typed into shell
      int number_of_words = tokens_get_length(tokens);
      char cmd[1024];
      strcpy(cmd, tokens_get_token(tokens, 0));

      // Converting shell prompt into string array to pass in as arguments.
      // Also checks for redirection
      char *args[number_of_words+1];
      int num_word_of_redirect;
      bool redirect_output = false; 
      bool redirect_input = false; 
      bool background = false;    
      char redirection[1024]; 

      for (int i = 0; i < number_of_words; i++) {
        strcpy(redirection, tokens_get_token(tokens, i));
        if (redirection[0] == '>') {
          redirect_output = true;
          num_word_of_redirect = i;
          break;
        } else if (redirection[0] == '<') {
          redirect_input = true;
          num_word_of_redirect = i;
          break;
        } else if (redirection[0] == '&' && i == (number_of_words - 1)){
          background = true;
          num_word_of_redirect = i;
          break;
        } else {
          args[i] = tokens_get_token(tokens, i);
        }
      }
      if (redirect_output || redirect_input || background) {
        args[num_word_of_redirect] = NULL;
      } else {
        args[number_of_words] = NULL;
      }

      // If file path not included, check for it in PATH
      if (cmd[0] != '/') {
        char final_path[1024];
        char a_path[1024];
        char cmd_path[1024] = "/";
        
        strcat(cmd_path, cmd);
        int string_ctr = 0;
        
        char path[1024];
        strcpy(path, getenv("PATH"));
        int len_of_path = strlen(path);

        int exists;

        for (int i = 0; i < len_of_path; i++) {
          if (path[i] == ':') {                           // Reach the end of a path, check if cmd exists there
            strcpy(final_path, a_path);
            strcat(final_path, cmd_path);
            exists = access(final_path, F_OK);
            if (exists == 0) {                            // If exists, we found our path and break out of loop
              break;
            } else {
              string_ctr = 0;
              strncpy(final_path, "", sizeof(final_path));
              strncpy(a_path, "", sizeof(a_path));        // If not existing, we reset final_path/a_path to be empty 
            }
          } 

          else {                                          // Build the path for one of the PATH paths
            a_path[string_ctr] = path[i];
            string_ctr++;
          }
        }
        memset(cmd, 0, sizeof(cmd));
        strcpy(cmd, final_path);
      }

      // Get the file descriptor if redirecting standard output
      int newfd;
      char redirect_file[1024];
      if (redirect_output) {
        strcpy(redirect_file, tokens_get_token(tokens, num_word_of_redirect + 1));
        if ((newfd = open(redirect_file, O_CREAT|O_TRUNC|O_WRONLY, 0644)) < 0) {
          perror(redirect_file);
          exit(1);
        }
      }

      if (redirect_input) {
        strcpy(redirect_file, tokens_get_token(tokens, num_word_of_redirect + 1));
        if ((newfd = open(redirect_file, O_RDONLY)) < 0) {
          perror(redirect_file);
          exit(1);
        }
      }

      pid_t pid;
      int status;

      pid = fork();
      if (pid == 0) {
        if (redirect_output) {
          dup2(newfd, STDOUT_FILENO);
          close(newfd);
        } else if (redirect_input) {
          dup2(newfd, STDIN_FILENO);
          close(newfd);
        }
        // if (background) {
        //   signal(SIGCHLD, signal_handler);
        // }
        execv(cmd, args);
        perror(cmd);
        exit(0); 
      }
      if (!background) {
        wait(&status);
      }
      memset(redirect_file, 0, sizeof(redirect_file));
    }

    if (shell_is_interactive)
      /* Please only print shell prompts when standard input is not a tty */
      fprintf(stdout, "%d: ", ++line_num);

    /* Clean up memory */
    tokens_destroy(tokens);
    
  }

  return 0;
}
