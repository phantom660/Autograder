#include "utils.h"

#define MAX_STRING_SIZE 1024            // Macro for max size of a string


const char* get_status_message(int status) {
    switch (status) {
        case CORRECT: return "correct";
        case INCORRECT: return "incorrect";
        case SEGFAULT: return "crash";
        case STUCK_OR_INFINITE: return "stuck/inf";
        default: return "unknown";
    }
}


char *get_exe_name(char *path) {
    return strrchr(path, '/') + 1;
}


char **get_student_executables(char *solution_dir, int *num_executables) {
    DIR *dir;
    struct dirent *entry;
    struct stat st;

    // Open the directory
    dir = opendir(solution_dir);
    if (!dir) {
        perror("Failed to open directory");
        exit(EXIT_FAILURE);
    }

    // Count the number of executables
    *num_executables = 0;
    while ((entry = readdir(dir)) != NULL) {
        // Ignore hidden files
        char path[PATH_MAX];
        sprintf(path, "%s/%s", solution_dir, entry->d_name);
        
        if (stat(path, &st) == 0) {
            if (S_ISREG(st.st_mode) && entry->d_name[0] != '.')
                (*num_executables)++;
        } 
        else {
            perror("Failed to get file status");
            exit(EXIT_FAILURE);
        }
    }

    // Allocate memory for the array of strings
    char **executables = (char **) malloc(*num_executables * sizeof(char *));

    // Reset the directory stream
    rewinddir(dir);

    // Read the file names
    int i = 0;
    while ((entry = readdir(dir)) != NULL) {
        // Ignore hidden files
        char path[PATH_MAX];
        sprintf(path, "%s/%s", solution_dir, entry->d_name);

        if (stat(path, &st) == 0) {
            if (S_ISREG(st.st_mode) && entry->d_name[0] != '.') {
                executables[i] = (char *) malloc((strlen(solution_dir) + strlen(entry->d_name) + 2) * sizeof(char));
                sprintf(executables[i], "%s/%s", solution_dir, entry->d_name);
                i++;
            }
        }
    }

    // Close the directory
    closedir(dir);

    // Return the array of strings (remember to free the memory later)
    return executables;
}


// TODO: Implement this function
int get_batch_size() { 
    int pipe_fds[2];
    int ret_val = pipe(pipe_fds);  // Create a pipe
    if (ret_val == -1) {
        perror("Error in pipe\n");
        exit(-1);
    }


    pid_t pid = fork(); // Create a child process

    if (pid == 0) {  // Child process
        close(pipe_fds[0]);
        dup2(pipe_fds[1], 1);
        // printf("Hello\n");
        execlp("grep", "grep", "processor", "/proc/cpuinfo",NULL);  // Execute grep command to find the number of processors
        perror("error in grep");
        close(pipe_fds[1]);
        exit(1);
    } else if (pid > 0) { // Parent process
        int counter = 0;
        close(pipe_fds[1]);
        char c;
        while(read(pipe_fds[0], &c, 1) > 0) {
            if (c == '\n') {
                counter ++;
            }
        }
        close(pipe_fds[0]);
        return counter;
    } else {
        perror("fork failed");
        exit(1);
    }
}


// TODO: Implement this function
void create_input_files(char **argv_params, int num_parameters) {

    // For all parameters, create a new file in input/<parameter>.in file
    // Write the parameter to the file

    for(int i = 0; i < num_parameters; i++) { //loop through all parameters
        char in_file_name[MAX_STRING_SIZE] = ""; //initialize a char array to store the file name
        snprintf(in_file_name, sizeof(in_file_name), "input/%s.in", argv_params[i]); //create the file name
        FILE* fh = fopen(in_file_name, "w");

        if (!fh) {
            perror("Failed to open file");
            exit(1);
        }

        fprintf(fh, "%s", argv_params[i]);
        fclose(fh);
    }
}


// TODO: Implement this function
void start_timer(int seconds, void (*timeout_handler)(int)) {

    // Raise the SIGALRM signal to trigget an alarm after <seconds> seconds
    // timeout_handler handles the case when signal is raised

    signal(SIGALRM, timeout_handler); //set the signal handler
    alarm(seconds); //set the alarm
}


// TODO: Implement this function
void cancel_timer() {
    alarm(0); //cancel the alarm
}


// TODO: Implement this function
void remove_input_files(char **argv_params, int num_parameters) {

    // For all parameters, generate the name for the file where it's stored (input/<parameter>.in file)
    // And unlinks the file to remove it from directory

    for(int i = 0; i < num_parameters; i ++) { //loop through all parameters
        char in_file_name[MAX_STRING_SIZE] = ""; //initialize a char array to store the file name
        snprintf(in_file_name, sizeof(in_file_name), "input/%s.in", argv_params[i]);
        if (unlink(in_file_name) == -1) { //unlink the file
            perror("Unlink error");
        }
    }
}


// TODO: Implement this function
void remove_output_files(autograder_results_t *results, int tested, int current_batch_size, char *param) {

    // For every executable-parameter pair in current batch, 
    // generate the name for the file where output is stored (input/<executable>.<parameter> file)
    // And unlinks the file to remove it from directory

    for (int i = 0; i < current_batch_size; i ++) { //loop through all executables
        char out_file_name[MAX_STRING_SIZE] = ""; //initialize a char array to store the file name
        sprintf(out_file_name, "output/%s.%s", get_exe_name(results[tested - current_batch_size + i].exe_path), param);
        if (unlink(out_file_name) == -1) { //unlink the file
            perror("Unlink error");
        }
    }
}


int get_longest_len_executable(autograder_results_t *results, int num_executables) {  //get the length of the longest executable name
    int longest_len = 0; //initialize the length of the longest executable name
    for (int i = 0; i < num_executables; i++) { //loop through all executables
        char *exe_name = get_exe_name(results[i].exe_path); //get the executable name
        int len = strlen(exe_name); //get the length of the executable name
        if (len > longest_len) { //if the length is greater than the longest length
            longest_len = len;
        }
    }
    return longest_len;
}
 

void write_results_to_file(autograder_results_t *results, int num_executables, int total_params) {
    FILE *file = fopen("results.txt", "w");
    if (!file) {
        perror("Failed to open file");
        return;
    }

    // Find the longest executable name (for formatting purposes)
    int longest_len = 0;
    for (int i = 0; i < num_executables; i++) {
        char *exe_name = get_exe_name(results[i].exe_path);
        int len = strlen(exe_name);
        if (len > longest_len) {
            longest_len = len;
        }
    }

    // Sort the results data structure by executable name (specifically number at the end)
    for (int i = 0; i < num_executables; i++) {
        for (int j = i + 1; j < num_executables; j++) {
            char *exe_name_i = get_exe_name(results[i].exe_path);
            int num_i = atoi(strrchr(exe_name_i, '_') + 1);
            char *exe_name_j = get_exe_name(results[j].exe_path);
            int num_j = atoi(strrchr(exe_name_j, '_') + 1);
            if (num_i > num_j) {
                autograder_results_t temp = results[i];
                results[i] = results[j];
                results[j] = temp;
            }
        }
    }

    // Write results to file
    for (int i = 0; i < num_executables; i++) {
        char *exe_name = get_exe_name(results[i].exe_path);

        char format[20];
        sprintf(format, "%%-%ds:", longest_len);
        fprintf(file, format, exe_name); // Write the program path
        for (int j = 0; j < total_params; j++) {
            fprintf(file, "%5d (", results[i].params_tested[j]); // Write the pi value for the program
            const char* message = get_status_message(results[i].status[j]);
            fprintf(file, "%9s) ", message); // Write each status
        }
        fprintf(file, "\n");
    }

    fclose(file);
}


// TODO: Implement this function
double get_score(char *results_file, char *executable_name) {
    FILE *file = fopen(results_file, "r");
    if (!file) {
        perror("Failed to open file");
        exit(1);
    }

    char res[11];
    char line[512];
    char *exe_name = get_exe_name(executable_name);     // Gets the executable name
    int l = atoi(strrchr(exe_name, '_') + 1);           // Gets the executable number

    if (l != 1) { // If the executable number is not 1, skip the first l-1 lines
        fgets(line, sizeof(line), file);
        if (l != 2) {
            fseek(file, strlen(line)*(l-1), SEEK_SET);
        }
    }

    fgets(line, sizeof(line), file); // Read the line with the results
    // printf("%s\n", line);
    int flag = 0;
    int correct = 0;
    int total = 0;

    memset(res, '\0', strlen(res)); //initialize the result array

    int j = 0;
    for(int i = 12; i < strlen(line); i ++) { //loop through the line
        if (line[i] == ')') { //if the character is ')'
            flag = 0;
            // printf("%s, ", res);
            if (strcmp(res, "  correct") == 0) { //if the result is correct
                correct ++;
            }
            total ++;
            memset(res, '\0', strlen(res)); //initialize the result array
            j = 0;
        }

        if (flag == 1) { 
            res[j] = line[i];
            j ++;
        }

        if (line[i] == '(') { 
            flag = 1;
        }

    }

    fclose(file);
    return (double) correct/total;
}


void write_scores_to_file(autograder_results_t *results, int num_executables, char *results_file) {
    for (int i = 0; i < num_executables; i++) {
        double student_score = get_score(results_file, results[i].exe_path);
        char *student_exe = get_exe_name(results[i].exe_path);

        char score_file[] = "scores.txt";

        FILE *score_fp;
        if (i == 0)
            score_fp = fopen(score_file, "w");
        else
            score_fp = fopen(score_file, "a");

        if (!score_fp) {
            perror("Failed to open score file");
            exit(1);
        }

        int longest_len = get_longest_len_executable(results, num_executables);

        char format[20];
        sprintf(format, "%%-%ds: ", longest_len);
        fprintf(score_fp, format, student_exe);
        fprintf(score_fp, "%5.3f\n", student_score);

        fclose(score_fp);
    }
}