#include "utils.h" //testing mofo testing ABCDE

// Batch size is determined at runtime now
pid_t *pids;
 
// Stores the results of the autograder (see utils.h for details) 
autograder_results_t *results;

int num_executables;      // Number of executables in test directory
int curr_batch_size;      // At most batch_size executables will be run at once
int total_params;         // Total number of parameters to test - (argc - 2)

// Contains status of child processes (-1 for done, 1 for still running)
int *child_status;

#define MAX_STRING_SIZE 1024        // Macro for max size of a string


// TODO: Timeout handler for alarm signal - kill remaining child processes
void timeout_handler(int signum) {
    for (int i = 0; i < curr_batch_size; i ++) {        // Checks for each process, if it is running at timeout
        if (child_status[i] == 1) {
            kill(pids[i], SIGKILL);                     // Kills the process if it is running at timeout
            child_status[i] = 2;                        // Changes status to 2 (this is used to handle the case and print "stuck/inf")
        }
    }
}


// Execute the student's executable using exec()
void execute_solution(char *executable_path, char *input, int batch_idx) { // batch_idx is the index of the executable in the current batch
    #ifdef PIPE
        // TODO: Setup pipe
        int pipe_fds[2];    // Array to store pipe file descriptors
        int ret_val = pipe(pipe_fds); // Create pipe
        if (ret_val == -1) {
            perror("Error in pipe\n");
            exit(-1);
        }

    #endif
    
    pid_t pid = fork();

    // Child process
    if (pid == 0) {
        char *executable_name = get_exe_name(executable_path);      // Get executable name from filepath
        char out_file_name[MAX_STRING_SIZE] = "";                   // Var for output file name

        sprintf(out_file_name, "output/%s.%s", executable_name, input);     // Generate the name for output file 

        // printf("%s\n", out_file_name);



        // TODO (Change 1): Redirect STDOUT to output/<executable>.<input> file

        // Open output file and redirect it to STDOUT using dup2
        int out_file = open(out_file_name, O_WRONLY|O_CREAT|O_TRUNC, 0644);

        if (out_file == -1) {       // Error checking for open()
            perror("Failed to open File");
            exit(1);
        }

        dup2(out_file, STDOUT_FILENO); // Redirect STDOUT to output file
        // printf("%s\n", input);
        // dup2(dup_stdout, 1);


        // TODO (Change 2): Handle different cases for input source
        #ifdef EXEC
            execl(executable_path, executable_name, input, NULL);   // Run exec, input passed as parameter

        #elif REDIR
            // TODO: Redirect STDIN to input/<input>.in file
            char in_file_name[MAX_STRING_SIZE];    // Var for input file name
            sprintf(in_file_name, "input/%s.in", input);     // Generate name for input file

            // Open input file and redirect it to stdin using dup2
            int in_file = open(in_file_name, O_RDONLY);

            if (in_file == -1) {
                perror("Failed to open file");
                exit(1);
            }

            dup2(in_file, STDIN_FILENO);

            execl(executable_path, executable_name, NULL);      // Run exec, input read from input file so not passed as parameter
        #elif PIPE
            close(pipe_fds[1]);
            char in[32] = "";
            sprintf(in, "%d", pipe_fds[0]);
            // TODO: Pass read end of pipe to child process
            execl(executable_path, executable_name, in, NULL);     // Run exec, input read from pipe so not passed as parameter

        #endif

        // If exec fails
        perror("Failed to execute program");
        exit(1);
    } 
    // Parent process
    else if (pid > 0) {
        #ifdef PIPE
            // TODO: Send input to child process via pipe
            int inp = atoi(input);     // Convert input to integer
            close(pipe_fds[0]);
            write(pipe_fds[1], &inp, sizeof(int));    // Write input to pipe
            close(pipe_fds[1]);
            
        #endif
        

        pids[batch_idx] = pid;
    }
    // Fork failed
    else {
        perror("Failed to fork");
        exit(1);
    }
}


// Wait for the batch to finish and check results
void monitor_and_evaluate_solutions(int tested, char *param, int param_idx) {  // Keep track of finished processes for alarm handler. 
    // tested is the number of executables tested so far, param is the current parameter being tested, param_idx is the index of the parameter in the results struct

    child_status = malloc(curr_batch_size * sizeof(int)); 
    /**
     * Sets the child_status array to 1 for each element in the range [0, curr_batch_size).
     *
     * @param curr_batch_size The size of the current batch.
     */
    for (int j = 0; j < curr_batch_size; j++) {
        child_status[j] = 1;
    }
    for (int j = 0; j < curr_batch_size; j++) { // main evaluation loop - wait until each process has finished or timed out
        child_status[j] = 1;
    }

    // MAIN EVALUATION LOOP: Wait until each process has finished or timed out
    for (int j = 0; j < curr_batch_size; j++) {

        int status;
        // pid_t pid = 
        waitpid(pids[j], &status, 0); // Wait for child process to finish

        // TODO: What if waitpid is interrupted by a signal?


        // TODO: Determine if the child process finished normally, segfaulted, or timed out

        // int exit_status = WEXITSTATUS(status);
        int exited = WIFEXITED(status);
        int signaled = WIFSIGNALED(status);

        int status_check;       // Var to store result (from output file) in case of correct/incorrect case
        char out_file_name[MAX_STRING_SIZE] = "";
        sprintf(out_file_name, "output/%s.%s", get_exe_name(results[tested - curr_batch_size + j].exe_path), param);    // Generate name for output file
        FILE* fh = fopen(out_file_name, "r");

        if (!fh) {
            perror("Failed to open file");
            exit(1);
        }

        fscanf(fh, "%d", &status_check);        // Reads the status from the output file in case of correct/incorrect case

        if (child_status[j] == 2) {         // Process timed out and status changed to 2 in timeout handler
            results[tested - curr_batch_size + j].status[param_idx] = 4;        // Case 4 - stuck/infloop
            child_status[j] = -1;       // Status changed to -1 indicating process has been killed so not running anymore
        }
        if (signaled && WTERMSIG(status) == SIGSEGV) {          // If process is interrupted by SIGSEGV signal, its a segfault
            results[tested - curr_batch_size + j].status[param_idx] = 3;        // Case 3 - segfault
        } 
        // else if (signaled && WTERMSIG(status) == SIGALRM) {
        //     results[tested - curr_batch_size + j].status[param_idx] = 4;
        // }
        else if (exited && (status_check == 0 || status_check == 1)) {           // If exited normally, correct or incorrect (status read from output file)
            results[tested - curr_batch_size + j].status[param_idx] = status_check + 1;         // Case 1 - correct (status 0 + 1 = 1) or Case 2 - incorrect (status 1 + 1 = 2)
        } 
        // else {        // Timeout
        //     results[tested - curr_batch_size + j].status[param_idx] = 4;
        // }
        
        
        // TODO: Also, update the results struct with the status of the child process

        // NOTE: Make sure you are using the output/<executable>.<input> file to determine the status
        //       of the child process, NOT the exit status like in Project 1.


        // Adding tested parameter to results struct
        results[tested - curr_batch_size + j].params_tested[param_idx] = atoi(param); // Convert param to integer and store in results struct

        // Mark the process as finished
        child_status[j] = -1;
    }

    free(child_status);
}



int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <testdir> <p1> <p2> ... <pn>\n", argv[0]);
        return 1;
    }

    char *testdir = argv[1];
    total_params = argc - 2;

    // TODO (Change 0): Implement get_batch_size() function
    int batch_size = get_batch_size(); // Implement this function (src/utils.c)

    char **executable_paths = get_student_executables(testdir, &num_executables);

    // Construct summary struct
    results = malloc(num_executables * sizeof(autograder_results_t));
    for (int i = 0; i < num_executables; i++) { // Initialize results struct for each executable in test directory 
        results[i].exe_path = executable_paths[i]; // Initialize exe_path for each executable
        results[i].params_tested = malloc((total_params) * sizeof(int)); // Initialize params_tested array for each executable
        results[i].status = malloc((total_params) * sizeof(int)); // Initialize status array for each executable
    }

    #ifdef REDIR
        // TODO: Create the input/<input>.in files and write parameters to them     
        create_input_files(argv + 2, total_params);  // Implement this function (src/utils.c)
    #endif
    
    // MAIN LOOP: For each parameter, run all executables in batch size chunks
    for (int i = 2; i < argc; i++) {
        int remaining = num_executables;
        int tested = 0;

        // Test the parameter on each executable
        while (remaining > 0) {

            // Determine current batch size - min(remaining, batch_size)
            curr_batch_size = remaining < batch_size ? remaining : batch_size;
            pids = malloc(curr_batch_size * sizeof(pid_t));
        
            // Execute the programs in batch size chunks
            for (int j = 0; j < curr_batch_size; j++) {
                execute_solution(executable_paths[tested], argv[i], j);
                tested++;
            }

            // TODO (Change 3): Setup timer to determine if child process is stuck
            start_timer(TIMEOUT_SECS, timeout_handler);  // Implement this function (src/utils.c)

            // Wait for the batch to finish and check results
            monitor_and_evaluate_solutions(tested, argv[i], i - 2);

            // TODO: Cancel the timer if all child processes have finished
            if (child_status == NULL) {
                cancel_timer();  // Implement this function (src/utils.c)
            }

            // TODO Unlink all output files in current batch (output/<executable>.<input>)
            remove_output_files(results, tested, curr_batch_size, argv[i]);  // Implement this function (src/utils.c)
            

            // Adjust the remaining count after the batch has finished
            remaining -= curr_batch_size;
        }
    }

    #ifdef REDIR
        // TODO: Unlink all input files for REDIR case (<input>.in)
        remove_input_files(argv + 2, total_params);  // Implement this function (src/utils.c)
    #endif

    write_results_to_file(results, num_executables, total_params);

    // You can use this to debug your scores function
    // get_score("results.txt", results[0].exe_path);

    // Print each score to scores.txt
    write_scores_to_file(results, num_executables, "results.txt");

    // Free the results struct and its fields
    for (int i = 0; i < num_executables; i++) {
        free(results[i].exe_path);
        free(results[i].params_tested);
        free(results[i].status);
    }

    free(results);
    free(executable_paths);

    free(pids);
    
    return 0;
}