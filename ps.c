#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

#define MAX_LINE 1024
#define MAX_ARGS 64
#define MAX_VARS 100
#define MAX_VAR_NAME 50
#define MAX_VAR_VALUE 256
#define INPUT_SIZE 521

typedef struct {
    char name[MAX_VAR_NAME];
    char value[MAX_VAR_VALUE];
} var_t;

var_t shell_vars[MAX_VARS];
int var_count = 0;

// --- Variabele management ---
void init_shell_vars() {
    strcpy(shell_vars[var_count].name, "0");  // $0
    strcpy(shell_vars[var_count].value, "polshell");
    var_count++;

    // $USER → haal uit environment
    char *user = getenv("USER");
    if (!user) user = "unknown";
    strcpy(shell_vars[var_count].name, "USER");
    strncpy(shell_vars[var_count].value, user, MAX_VAR_VALUE-1);
    shell_vars[var_count].value[MAX_VAR_VALUE-1] = '\0';
    var_count++;

    // $PATH → haal uit environment
    char *path = getenv("PATH");
    if (!path) path = "";
    strcpy(shell_vars[var_count].name, "PATH");
    strncpy(shell_vars[var_count].value, path, MAX_VAR_VALUE-1);
    shell_vars[var_count].value[MAX_VAR_VALUE-1] = '\0';
    var_count++;
}

char* get_var_value(const char* name) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(shell_vars[i].name, name) == 0) {
            return shell_vars[i].value;
        }
    }
    return NULL;
}

// --- Variabele-substitutie voor alle $VAR ---
void substitute_vars(char **args, int arg_count) {
    for (int i = 0; i < arg_count; i++) {
        if (args[i][0] == '$') {
            char *val = get_var_value(args[i] + 1); // skip '$'
            if (val) {
                strcpy(args[i], val);
            } else {
                args[i][0] = '\0'; // onbekende variable → leeg
            }
        }
    }
}

/* -----------------------------
   simpele argv parser
   splitst op spaties (bewust simpel)
----------------------------- */
void parse_args(char *line, char **argv)
{
    int i = 0;

    char *tok = strtok(line, " \t\n");
    while (tok && i < MAX_ARGS - 1) {
        argv[i++] = tok;
        tok = strtok(NULL, " \t\n");
    }

    argv[i] = NULL;
}


/* -----------------------------
   voer 1 commando uit (geen pipe)
----------------------------- */
void run_single(char **argv)
{
    pid_t pid = fork();

    if (pid == 0) {
        execvp(argv[0], argv);
        perror("exec gefaald");
        _exit(127);
    }

    waitpid(pid, NULL, 0);
}


/* -----------------------------
   voer a | b uit
----------------------------- */
void run_pipe(char **left, char **right)
{
    int fds[2];

    if (pipe(fds) == -1) {
        perror("pipe");
        return;
    }

    pid_t p1 = fork();

    if (p1 == 0) {
        /* child 1 -> schrijft naar pipe */
        dup2(fds[1], STDOUT_FILENO);
        close(fds[0]);
        close(fds[1]);
        execvp(left[0], left);
        perror("exec links");
        _exit(127);
    }

    pid_t p2 = fork();

    if (p2 == 0) {
        /* child 2 -> leest van pipe */
        dup2(fds[0], STDIN_FILENO);
        close(fds[0]);
        close(fds[1]);
        execvp(right[0], right);
        perror("exec rechts");
        _exit(127);
    }

    /* parent */
    close(fds[0]);
    close(fds[1]);

    waitpid(p1, NULL, 0);
    waitpid(p2, NULL, 0);
}


/* -----------------------------
   main loop met variabele support
----------------------------- */
int main(void)
{
    char line[MAX_LINE];

    // init shell variables ($0)
    init_shell_vars();

    while (1) {
        printf("$ ");  // prompt
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin))
            break;

        if (line[0] == '\n')
            continue;

        /* check pipe */
        char *pipe_pos = strchr(line, '|');

        if (pipe_pos) {
            *pipe_pos = '\0';

            char *left_cmd = line;
            char *right_cmd = pipe_pos + 1;

            char *left_argv[MAX_ARGS];
            char *right_argv[MAX_ARGS];

            parse_args(left_cmd, left_argv);
            parse_args(right_cmd, right_argv);

            // variabelen substitutie
            substitute_vars(left_argv, MAX_ARGS);
            substitute_vars(right_argv, MAX_ARGS);

            run_pipe(left_argv, right_argv);
            continue;
        }

        /* normale command */
        char *argv[MAX_ARGS];
        parse_args(line, argv);

        if (!argv[0])
            continue;

        // variabelen substitutie
        int argc = 0;
        while (argv[argc]) argc++;
        substitute_vars(argv, argc);

        /* builtins */
        if (strcmp(argv[0], "exit") == 0)
            break;

        if (strcmp(argv[0], "cd") == 0) {
            if (!argv[1]) {
                fprintf(stderr, "cd: geef argumenten, wtf is u probleem\n");
            } else if (chdir(argv[1]) != 0) {
                perror("cd");
            }
            continue;
        }

        /* built-in: echo */
        if (strcmp(argv[0], "echo") == 0) {
            for (int i = 1; argv[i]; i++) {
                if (argv[i][0] != '\0') {  // skip onbekende vars
                    printf("%s", argv[i]);
                    if (argv[i + 1]) printf(" ");
                }
            }
            printf("\n");
            continue;
        }

        run_single(argv);
    }

    return 0;
}

