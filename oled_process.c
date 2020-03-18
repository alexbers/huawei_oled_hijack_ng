#define _GNU_SOURCE
#include <stdlib.h>
#include <fcntl.h>

#include <sys/wait.h>

void destroy_process();
static void proccess_poll();

extern uint32_t (*timer_create_ex)(uint32_t, uint32_t, void (*)(), uint32_t);
extern uint32_t (*timer_delete_ex)(uint32_t);

// ------------------------------ TASK LAUNCHING AND CONTROL LOGIC --------

long process_output_fd = -1;
pid_t child_pid = 0;
uint32_t process_pooling_timer = 0;
const int PROC_BUF_SIZE = 2048;
char process_data_buf[PROC_BUF_SIZE + 1] = {0};
int process_data_len = 0;
void (*process_callback)(int, char *) = 0;


int create_process(char* command, void (*finish_callback)(int, char *)) {
    if(child_pid) {
        fprintf(stderr, "Attempted to create process where another one is alive, killing it\n");
        destroy_process();
    }
    process_data_len = 0;
    process_data_buf[0] = 0;

    int pipe_fd[2] = {};

    if (pipe2(pipe_fd, O_CLOEXEC | O_NONBLOCK) != 0) {
        fprintf(stderr, "Failed to create pipe\n");
        return 1;
    }

    pid_t fork_result = fork();

    if (fork_result == -1) {
        fprintf(stderr, "Failed to fork\n");
        return 1;
    }

    if (fork_result == 0) {
        // child
        if (dup2(pipe_fd[1], 1) ==  -1) {
            fprintf(stderr, "Failure on dup2 call\n");
            exit(1);
        }

        execl("/bin/sh", "/bin/sh", "-c", command, NULL);
        exit(1);
    }

    // parent
    close(pipe_fd[1]);
    if (!process_pooling_timer) {
        process_pooling_timer = timer_create_ex(25, 1, proccess_poll, 0);
    }
    child_pid = fork_result;
    process_output_fd = pipe_fd[0];
    process_callback = finish_callback;
    return 0;
}

void process_consume_data() {
    if (child_pid == 0) {
        return;
    }

    if (process_data_len < PROC_BUF_SIZE) {
        ssize_t read_result = read(process_output_fd, process_data_buf + process_data_len,
                                   PROC_BUF_SIZE - process_data_len);

        if (read_result > 0) {
            process_data_len += read_result;
            process_data_buf[process_data_len] = 0;
        }
    } else {
        const int BUF_LEN = 64;
        char buf[BUF_LEN];
        // swallow the output
        read(process_output_fd, buf, BUF_LEN);
    }
}

static void proccess_poll() {
    if (child_pid == 0) {
        destroy_process();
        return;
    }

    process_consume_data();

    int wstatus = 0;
    pid_t pid = waitpid(child_pid, &wstatus, WNOHANG);

    if(pid > 0) {
        process_consume_data();
        void (*saved_callback)(int, char *) = process_callback;
        destroy_process();

        if (saved_callback) {
            int good = (WIFEXITED(wstatus) && !WIFSIGNALED(wstatus) && WEXITSTATUS(wstatus) == 0);
            saved_callback(good, process_data_buf);
        }
    } else if (pid == -1 && errno == ECHILD) {
        child_pid = 0;
        destroy_process();
    }
}

void destroy_process() {
    if (child_pid) {
        int wstatus = 0;

        kill(child_pid, SIGKILL);
        waitpid(child_pid, &wstatus, 0);

        child_pid = 0;
    }
    if(process_output_fd != -1) {
        close(process_output_fd);
        process_output_fd = -1;
    }
    process_callback = 0;
}

void destroy_process_pooler() {
    // the timers implemented as threads, so destroy them as rare as possible
    if(process_pooling_timer) {
        timer_delete_ex(process_pooling_timer);
        process_pooling_timer = 0;
    }
}
