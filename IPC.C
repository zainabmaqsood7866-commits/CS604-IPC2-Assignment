#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

struct Result {
    int total_time;
    int longest_task;
    int maximum_time;
};

// Read all required data from a pipe
int read_all(int fd, void *buffer, size_t size) {
    char *ptr = buffer;
    size_t count = 0;

    while (count < size) {
        ssize_t n = read(fd, ptr + count, size - count);

        if (n <= 0)
            return -1;

        count += n;
    }

    return 0;
}

// Write all data into a pipe
int write_all(int fd, const void *buffer, size_t size) {
    const char *ptr = buffer;
    size_t count = 0;

    while (count < size) {
        ssize_t n = write(fd, ptr + count, size - count);

        if (n <= 0)
            return -1;

        count += n;
    }

    return 0;
}

int main() {
    char studentID[64];
    int times[3];

    int pipe1[2], pipe2[2];
    struct Result result;

    printf("Enter Student ID: ");
    if (scanf("%63s", studentID) != 1) {
        printf("Invalid Student ID.\n");
        return 1;
    }

    // Get execution times from the user
    for (int i = 0; i < 3; i++) {
        printf("Enter Task %d Execution Time: ", i + 1);

        if (scanf("%d", &times[i]) != 1 || times[i] <= 0) {
            printf("Please enter a positive integer.\n");
            return 1;
        }
    }

    // Create two pipes
    if (pipe(pipe1) == -1 || pipe(pipe2) == -1) {
        perror("Pipe creation failed");
        return 1;
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("Fork failed");
        return 1;
    }

    if (pid == 0) {

        // Child process
        close(pipe1[1]);
        close(pipe2[0]);

        int task_times[3];

        // Receive task times from parent
        if (read_all(pipe1[0], task_times, sizeof(task_times)) == -1) {
            perror("Child read failed");
            _exit(1);
        }

        close(pipe1[0]);

        // Calculate total execution time
        result.total_time = 0;

        result.maximum_time = task_times[0];
        result.longest_task = 1;

        for (int i = 0; i < 3; i++) {

            result.total_time += task_times[i];

            // Update longest task when a larger time is found
            if (task_times[i] > result.maximum_time) {
                result.maximum_time = task_times[i];
                result.longest_task = i + 1;
            }
        }

        // Send results back to parent
        if (write_all(pipe2[1], &result, sizeof(result)) == -1) {
            perror("Child write failed");
            close(pipe2[1]);
            _exit(1);
        }

        close(pipe2[1]);
        _exit(0);
    }

    else {

        // Parent process
        close(pipe1[0]);
        close(pipe2[1]);

        // Send all three task times to child
        if (write_all(pipe1[1], times, sizeof(times)) == -1) {
            perror("Parent write failed");
            close(pipe1[1]);
            close(pipe2[0]);
            waitpid(pid, NULL, 0);
            return 1;
        }

        close(pipe1[1]);

        // Receive calculated results from child
        if (read_all(pipe2[0], &result, sizeof(result)) == -1) {
            perror("Parent read failed");
            close(pipe2[0]);
            waitpid(pid, NULL, 0);
            return 1;
        }

        close(pipe2[0]);

        waitpid(pid, NULL, 0);

        // Display task processing report
        char report[1024];

        int length = snprintf(report, sizeof(report),
            "\n--- Task Processing Report ---\n"
            "Student ID: %s\n"
            "Task 1: %d units\n"
            "Task 2: %d units\n"
            "Task 3: %d units\n"
            "Total Execution Time: %d units\n"
            "Longest Task: Task %d\n"
            "Maximum Execution Time: %d units\n",
            studentID,
            times[0], times[1], times[2],
            result.total_time,
            result.longest_task,
            result.maximum_time
        );

        printf("%s", report);

        // Save the same report in a text file
        char filename[100];

        snprintf(filename, sizeof(filename),
                 "%s_report.txt", studentID);

        int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);

        if (fd < 0) {
            perror("File opening failed");
            return 1;
        }

        if (write_all(fd, report, length) == -1) {
            perror("File writing failed");
            close(fd);
            return 1;
        }

        close(fd);

        printf("Report saved to: %s\n", filename);
    }

    return 0;
}
