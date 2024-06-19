#include "utils.h"

// Run the (executable, parameter) pairs in batches of 8 to avoid timeouts due to 
// having too many child processes running at once
#define PAIRS_BATCH_SIZE 8

#define MAX_STRING_SIZE 1024            // Macro for max size of a string

typedef struct {
    char *executable_path;
    int parameter;
    int status;
} pairs_t;

// Store the pairs tested by this worker and the results
pairs_t *pairs;

// Information about the child processes and their results
pid_t *pids;
int *child_status;     // Contains status of child processes (-1 for done, 1 for still running)

int curr_batch_size;   // At most PAIRS_BATCH_SIZE (executable, parameter) pairs will be run at once
long worker_id;        // Used for sending/receiving messages from the message queue


// TODO: Timeout handler for alarm signal - should be the same as the one in autograder.c
void timeout_handler(int signum) {
    for (int i = 0; i < curr_batch_size; i ++) {        // Checks for each process, if it is running at timeout
        if (child_status[i] == 1) {
            kill(pids[i], SIGKILL);                     // Kills the process if it is running at timeout
            child_status[i] = 2;                        // Changes status to 2 (this is used to handle the case and print "stuck/inf")
        }
    }
}


// Execute the student's executable using exec()
void execute_solution(char *executable_path, int param, int batch_idx) {

    // printf("here\n");
 
    pid_t pid = fork();

    // Child process
    if (pid == 0) {
        
        char *executable_name = get_exe_name(executable_path);
        char out_file_name[MAX_STRING_SIZE];                   // Var for output file name

        // printf("Here\n");
        // printf("%s %d\n", executable_name, param);
        sprintf(out_file_name, "output/%s.%d", executable_name, param);     // Generate the name for output file 

        // printf("Here: %s %d\n", executable_name, param);


        // TODO: Redirect STDOUT to output/<executable>.<input> file

        // Open output file and redirect it to STDOUT using dup2
        int out_file = open(out_file_name, O_WRONLY|O_CREAT|O_TRUNC, 0644);

        if (out_file == -1) {       // Error checking for open()
            perror("Failed to open File");
            exit(1);
        }

        dup2(out_file, STDOUT_FILENO);

        char input[8];
        memset(input, '\0', 8);
        sprintf(input, "%d", param);


        // TODO: Input to child program can be handled as in the EXEC case (see template.c)
        execl(executable_path, executable_name, input, NULL);   // Run exec, input passed as parameter
        
        perror("Failed to execute program in worker");
        exit(1);
    }
    // Parent process
    else if (pid > 0) {
        pids[batch_idx] = pid;
    }
    // Fork failed
    else {
        perror("Failed to fork");
        exit(1);
    }
}


// Wait for the batch to finish and check results
void monitor_and_evaluate_solutions(int finished) {
    // Keep track of finished processes for alarm handler
    child_status = malloc(curr_batch_size * sizeof(int));
    for (int j = 0; j < curr_batch_size; j++) {
        child_status[j] = 1;
    }

    // MAIN EVALUATION LOOP: Wait until each process has finished or timed out
    for (int j = 0; j < curr_batch_size; j++) {
        char *current_exe_path = pairs[finished + j].executable_path;
        int current_param = pairs[finished + j].parameter;

        // printf("%s %d\n", current_exe_path, current_param);

        int status;
        // pid_t pid = 
        waitpid(pids[j], &status, 0);

        // TODO: What if waitpid is interrupted by a signal?


        // int exit_status = WEXITSTATUS(status);
        int exited = WIFEXITED(status);
        int signaled = WIFSIGNALED(status);

        int status_check;       // Var to store result (from output file) in case of correct/incorrect case
        char out_file_name[MAX_STRING_SIZE] = "";
        sprintf(out_file_name, "output/%s.%d", get_exe_name(current_exe_path), current_param);
        FILE* fh = fopen(out_file_name, "r");

        if (!fh) {
            perror("Failed to open file");
            exit(1);
        }

        fscanf(fh, "%d", &status_check);        // Reads the status from the output file in case of correct/incorrect case

        if (child_status[j] == 2) {         // Process timed out and status changed to 2 in timeout handler
            pairs[finished + j].status = 4;        // Case 4 - stuck/infloop
            child_status[j] = -1;       // Status changed to -1 indicating process has been killed so not running anymore
        }
        if (signaled && WTERMSIG(status) == SIGSEGV) {          // If process is interrupted by SIGSEGV signal, its a segfault
            pairs[finished + j].status = 3;        // Case 3 - segfault
        }
        else if (exited && (status_check == 0 || status_check == 1)) {           // If exited normally, correct or incorrect (status read from output file)
            pairs[finished + j].status = status_check + 1;         // Case 1 - correct (status 0 + 1 = 1) or Case 2 - incorrect (status 1 + 1 = 2)
        }


        // TODO: Check if the process finished normally, segfaulted, or timed out and update the 
        //       pairs array with the results. Use the macros defined in the enum in utils.h for 
        //       the status field of the pairs_t struct (e.g. CORRECT, INCORRECT, SEGFAULT, etc.)
        //       This should be the same as the evaluation in autograder.c, just updating `pairs` 
        //       instead of `results`.


        // Mark the process as finished
        child_status[j] = -1;
    }
    // if (finished == 8) {
    //     printf("Wroker = %ld, i = %d\n", worker_id, finished);
    // }

    free(child_status);
}


// Send results for the current batch back to the autograder
void send_results(int msqid, long mtype, int finished) {

    msgbuf_t message;
    // printf("Worker = %ld b = %d\n", worker_id, curr_batch_size);
    // Format of message should be ("%s %d %d", executable_path, parameter, status)
    for (int j = 0; j < curr_batch_size; j++) {
        message.mtype = mtype;
        sprintf(message.mtext, "%s %d %d", pairs[finished + j].executable_path, pairs[finished + j].parameter, pairs[finished + j].status);
        // if (finished == 8) {
        //    printf("Worker: %2ld, j = %d, batch_size = %d, message = %s\n", worker_id, j, curr_batch_size, message.mtext); 
        // }
        // printf("Worker: %s\n", message.mtext);
        // message.msg_text[strcspn(message.msg_text, "\n")] = '\0'; // Remove newline
        if (msgsnd(msqid, &message, sizeof(message), 0) == -1) {
            perror("msgsnd");
            exit(1);
        }
        // if (finished == 8) {
        //    printf("Worker: %2ld, j = %d, batch_size = %d, message = %s\n", worker_id, j, curr_batch_size, message.mtext); 
        // }
    }

    // if (finished == 8) {
    //     printf("Wroker = %ld, i = %d\n", worker_id, finished);
    // }
}


// Send DONE message to autograder to indicate that the worker has finished testing
void send_done_msg(int msqid, long mtype) { //
    // printf("%ld\n", worker_id);
    msgbuf_t message; // message buffer
    message.mtype = mtype; // setting the message type
    // strcpy(message.mtext, "DONE");
    sprintf(message.mtext, "%s %d %d", "DONE", 0, 0);
    // printf("Worker: %s\n", message.mtext);
    if (msgsnd(msqid, &message, sizeof(message), 0) == -1) { //using message queue to send the message
        perror("msgsnd");
        exit(1);
    }
}


int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <msqid> <worker_id>\n", argv[0]);
        return 1;
    }

    int msqid = atoi(argv[1]); // gets the message queue id
    worker_id = atoi(argv[2]); // gets the worker id

    // TODO: Receive initial message from autograder specifying the number of (executable, parameter) 
    // pairs that the worker will test (should just be an integer in the message body). (mtype = worker_id)

    msgbuf_t message; //recieves the message from the message queue
    // this message will be stored in the message buffer
    if (msgrcv(msqid, &message, sizeof(message), worker_id, 0) == -1) {
        perror("msgrcv");
        exit(1);
    }

    int pairs_to_test = atoi(message.mtext);    // extract the text content from the message and convert it to integer
    

    // TODO: Parse message and set up pairs_t array
    pairs = malloc(pairs_to_test * sizeof(pairs_t)); // allocate memory for the pairs_t array

    for (int i = 0; i < pairs_to_test; i ++) { // loop to allocate memory for the executable path in the pairs_t array
        pairs[i].executable_path = malloc(MAX_STRING_SIZE*sizeof(char)); // allocate memory for the executable path
    }

    // TODO: Receive (executable, parameter) pairs from autograder and store them in pairs_t array.
    //       Messages will have the format ("%s %d", executable_path, parameter). (mtype = worker_id)

    for (int i = 0; i < pairs_to_test; i ++) { // loop to recieve the pairs from the message queue
        if (msgrcv(msqid, &message, sizeof(message), worker_id, 0) == -1) { // recieves the message from the message queue containing the pairs
            perror("msgrcv");
            exit(1);
        }

        char exe_name[100];
        int param;

        sscanf(message.mtext, "%s %d", exe_name, &param); // parse the message and store the pairs in the pairs_t array
        
        // printf("Here:\n");

        strcpy(pairs[i].executable_path, exe_name);
        pairs[i].parameter = param;

        // printf("%s %d; i = %d\n", pairs[i].executable_path, pairs[i].parameter, i);
    }

    // for (int i = 0; i < pairs_to_test; i ++) {
    //     printf("This: %s %d; i = %d\n", pairs[i].executable_path, pairs[i].parameter, i);
    // }

    // TODO: Send ACK message to mq_autograder after all pairs received (mtype = BROADCAST_MTYPE)

    message.mtype = BROADCAST_MTYPE;
    strcpy(message.mtext, "ACK");
    if (msgsnd(msqid, &message, sizeof(message), 0) == -1) { // send the message to the message queue through mq_autograder
                                                             // the message will contain the text "ACK" and will be of type BROADCAST_MTYPE
                                                             
        perror("msgsnd");
        exit(1);
    }

    // TODO: Wait for SYNACK from autograder to start testing (mtype = BROADCAST_MTYPE).
    //       Be careful to account for the possibility of receiving ACK messages just sent.

    while (1) { // loop untill the SYNACK message is recieved from the autograder
        if (msgrcv(msqid, &message, sizeof(message), BROADCAST_MTYPE, 0) == -1) { // recieves the message from the message queue
            perror("msgrcv");
            exit(1);
        }
        if (strcmp(message.mtext, "SYNACK") == 0) { // checks if the message recieved is SYNACK
            // printf("Received SYNACK\n");
            break;
        } else if (strcmp(message.mtext, "ACK") == 0) { // if no then send the ACK message again
            message.mtype = BROADCAST_MTYPE;
            strcpy(message.mtext, "ACK"); // send the ACK message to the message queue
            if (msgsnd(msqid, &message, sizeof(message), 0) == -1) { // send the message to the message queue through mq_autograder
                perror("msgsnd");
                exit(1);
            }
        }
        sleep(1); // sleep for 1 second before checking for the message again
    }

    // printf("%d\n", pairs_to_test);

    // Run the pairs in batches of 8 and send results back to autograder
    for (int i = 0; i < pairs_to_test; i+= PAIRS_BATCH_SIZE) {
        int remaining = pairs_to_test - i;
        curr_batch_size = remaining < PAIRS_BATCH_SIZE ? remaining : PAIRS_BATCH_SIZE;
        pids = malloc(curr_batch_size * sizeof(pid_t));

        for (int j = 0; j < curr_batch_size; j++) {
            // TODO: Execute the student executable
            // printf("This: %s %d; i = %d; j = %d\n", pairs[i+j].executable_path, pairs[i+j].parameter, i, j);
            execute_solution(pairs[i + j].executable_path, pairs[i + j].parameter, j); // execute the solution for the pairs
        }

        // TODO: Setup timer to determine if child process is stuck
        start_timer(TIMEOUT_SECS, timeout_handler);  // Implement this function (src/utils.c)

        // if (i == 8) {
        //     printf("Wroker = %ld, i = %d, ptt = %d\n", worker_id, i, pairs_to_test);
        // }

        // TODO: Wait for the batch to finish and check results
        monitor_and_evaluate_solutions(i);
        

        // TODO: Cancel the timer if all child processes have finished
        if (child_status == NULL) { // if all child processes have finished, cancel the timer
            cancel_timer();
        }

        // TODO: Send batch results (intermediate results) back to autograder
        send_results(msqid, worker_id, i); // send the results to the autograder

        free(pids);
    }

    // printf("%ld\n", worker_id);

    // TODO: Send DONE message to autograder to indicate that the worker has finished testing
    send_done_msg(msqid, worker_id); // send the DONE message to the autograder

    // printf("Here\n");

    // Free the pairs_t array
    // for (int i = 0; i < pairs_to_test; i++) { // free the memory allocated for the pairs_t array
    //     free(pairs[i].executable_path);
    // }
    free(pairs);

    // free(pids);
}