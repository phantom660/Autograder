#include "utils.h"

pid_t *workers;          // Workers determined by batch size
int *worker_done;        // 1 for done, 0 for still running

// Stores the results of the autograder (see utils.h for details)
autograder_results_t *results;

#define MAX_STRING_SIZE 1024            // Macro for max size of a string

int num_executables;      // Number of executables in test directory
int total_params;         // Total number of parameters to test - (argc - 2)
int num_workers;          // Number of workers to spawn


void launch_worker(int msqid, int pairs_per_worker, int worker_id) {
    msgbuf_t message;
    
    pid_t pid = fork();

    // Child process
    if (pid == 0) {

        // TODO: exec() the worker program and pass it the message queue id and worker id.
        //       Use ./worker as the path to the worker program.

        char msqidS[16]; 
        char worker_idS[16]; 

        sprintf(msqidS, "%d", msqid);
        sprintf(worker_idS, "%d", worker_id);

        execl("./worker", "worker", msqidS, worker_idS, NULL); // Execute worker program

        perror("Failed to spawn worker");
        exit(1);
    } 
    // Parent process
    else if (pid > 0) {
        // TODO: Send the total number of pairs to worker via message queue (mtype = worker_id)

        message.mtype = worker_id; // set mtype to worker_id
        sprintf(message.mtext, "%d", pairs_per_worker); //convert the number of pairs to string and store in message
        if (msgsnd(msqid, &message, sizeof(message), 0) == -1) {
            perror("msgsnd");
            exit(1);
        }
        

        // Store the worker's pid for monitoring
        workers[worker_id - 1] = pid;
    }
    // Fork failed 
    else {
        perror("Failed to fork worker");
        exit(1);
    }
}


// TODO: Receive ACK from all workers using message queue (mtype = BROADCAST_MTYPE)
void receive_ack_from_workers(int msqid, int num_workers) {
    msgbuf_t message; // message buffer
    for (int i = 0; i < num_workers; i ++) { // loop through all workers
        if (msgrcv(msqid, &message, sizeof(message), BROADCAST_MTYPE, 0) == -1) { // receive message from message queue
            perror("msgrcv");
            exit(1);
        }
        // printf("%s\n", message.mtext);
        if (strcmp("ACK", message.mtext) != 0) { // check if message is not ACK
            perror("No acknowledgement");
            exit(1);
        }
    }
}


// TODO: Send SYNACK to all workers using message queue (mtype = BROADCAST_MTYPE)
void send_synack_to_workers(int msqid, int num_workers) {
    msgbuf_t message; // message buffer
    message.mtype = BROADCAST_MTYPE; // set mtype to BROADCAST_MTYPE
    for (int i = 0; i < num_workers; i ++) { // loop through all workers processes
        strcpy(message.mtext, "SYNACK"); // set the message text to SYNACK
        if (msgsnd(msqid, &message, sizeof(message), 0) == -1) { // send message to message queue
            perror("msgsnd");
            exit(1);
        }
    }
}


// Wait for all workers to finish and collect their results from message queue
void wait_for_workers(int msqid, int pairs_to_test, char **argv_params) {
    int received = 0;
    worker_done = malloc(num_workers * sizeof(int));
    msgbuf_t message;
    for (int i = 0; i < num_workers; i++) {
        worker_done[i] = 0;
    }

    while (received < pairs_to_test) { // loop until all results have been received
        for (int i = 0; i < num_workers; i++) { // loop through all workers
            // printf("%d\n", received);
            if (worker_done[i] == 1) { // if worker is done, move on to next worker
                continue;
            }

            // TODO: Receive results from worker and store them in the results struct.
            //       If message is "DONE", set worker_done[i] to 1 and break out of loop.
            //       Messages will have the format ("%s %d %d", executable_path, parameter, status)
            //       so consider using sscanf() to parse the message.

            // Check if worker has finished
            pid_t retpid = waitpid(workers[i], NULL, WNOHANG); // check if worker has finished
            
            int msgflg;
            if (retpid > 0)
                // Worker has finished and still has messages to receive
                msgflg = 0;
            else if (retpid == 0)
                // Worker is still running -> receive intermediate results
                msgflg = IPC_NOWAIT;
            else {
                // Error
                perror("Failed to wait for child process");
                exit(1);
            }

            
            while (1) { // loop until all messages have been received
                if (msgrcv(msqid, &message, sizeof(message), i+1, msgflg) == -1) { // receive message from message queue
                    continue; // no message to receive then continue to the next one
                    // perror("msgrcv");
                    // exit(1);
                }
                char executable[MAX_STRING_SIZE];
                int parameter;
                int status;
                sscanf(message.mtext, "%s %d %d", executable, &parameter, &status); // parse the message to extract executable, parameter and status

                // printf("%s %d\n", executable, parameter);

                if (strcmp(executable, "DONE") == 0) { // check if message indicates worker is done
                    // printf("D\n");
                    worker_done[i] = 1; // set it to done
                    break;
                }

                for (int j = 0; j < num_executables; j ++) {   // loop through all executables
                    if (strcmp(executable, results[j].exe_path) == 0) { // check if executable matches teh current executable
                        for (int k = 0; k < total_params; k ++) { // loop through all parameters tested for this executable
                            int argvparam = atoi(argv_params[k]); // convert the parameter to integer
                            // printf("%s %d\n", executable, argvparam);
                            if(argvparam == parameter){ // check if the parameter matches the current parameter and then update them
                                results[j].params_tested[k] = parameter;
                                results[j].status[k] = status;
                                break;
                            }
                        }
                        break;
                    }
                }
                received++;
            }
        }
       // received += num_workers;
    }
    free(worker_done);
}


int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <testdir> <p1> <p2> ... <pn>\n", argv[0]);
        return 1;
    }

    char *testdir = argv[1];
    total_params = argc - 2;

    char **executable_paths = get_student_executables(testdir, &num_executables);

    // Construct summary struct
    results = malloc(num_executables * sizeof(autograder_results_t));
    for (int i = 0; i < num_executables; i++) {
        results[i].exe_path = executable_paths[i];
        results[i].params_tested = malloc((total_params) * sizeof(int));
        results[i].status = malloc((total_params) * sizeof(int));
    }

    num_workers = get_batch_size();
    // Check if some workers won't be used -> don't spawn them
    if (num_workers > num_executables * total_params) {
        num_workers = num_executables * total_params;
    }
    workers = malloc(num_workers * sizeof(pid_t));

    // Create a unique key for message queue
    key_t key = IPC_PRIVATE;

    // TODO: Create a message queue
    int msqid; // message queue id
    msqid = msgget(key, 0666 | IPC_CREAT); // create message queue
    if (msqid == -1) {
        perror("msgget");
        exit(1);
    }

    int num_pairs_to_test = num_executables * total_params;
    
    // Spawn workers and send them the total number of (executable, parameter) pairs they will test
    for (int i = 0; i < num_workers; i++) {
        int leftover = num_pairs_to_test % num_workers - i > 0 ? 1 : 0;
        int pairs_per_worker = num_pairs_to_test / num_workers + leftover;

        // TODO: Spawn worker and send it the number of pairs it will test via message queue
        launch_worker(msqid, pairs_per_worker, i + 1); // launch worker
    }

    // Send (executable, parameter) pairs to workers
    int sent = 0;
    for (int i = 0; i < total_params; i++) {
        for (int j = 0; j < num_executables; j++) {
            msgbuf_t msg;
            long worker_id = sent % num_workers + 1;
            
            // TODO: Send (executable, parameter) pair to worker via message queue (mtype = worker_id)

            msg.mtype = worker_id; // set mtype to worker_id

            sprintf(msg.mtext, "%s %d", results[j].exe_path, atoi(argv[i+2]));  // construct the message containing the executable path and parameter

            // printf("%s\n", msg.mtext);
            if (msgsnd(msqid, &msg, sizeof(msg), 0) == -1) {  // send message to message queue
                perror("msgsnd");
                exit(1);
            }
            sent++;
        }
    }

    // TODO: Wait for ACK from workers to tell all workers to start testing (synchronization)
    receive_ack_from_workers(msqid, num_workers); // receive ACK from workers

    // TODO: Send message to workers to allow them to start testing
    send_synack_to_workers(msqid, num_workers); // send SYNACK to workers

    // TODO: Wait for all workers to finish and collect their results from message queue
    wait_for_workers(msqid, num_pairs_to_test, argv + 2); // wait for workers to finish and collect their results

    // TODO: Remove ALL output files (output/<executable>.<input>)
    for (int i = 0; i < total_params; i++) { // loop through all parameters and 
        for (int j = 0; j < num_executables; j++) { // loop through all executables
            char out_file_name[MAX_STRING_SIZE]; // construct the file path for the output file
            sprintf(out_file_name, "output/%s.%d", get_exe_name(results[j].exe_path), results[j].params_tested[i]); //
            // printf("%s\n", out_file_name);
            if (unlink(out_file_name) == -1) { // remove the output file
                perror("Unlink error");
            }
        }
    }

    write_results_to_file(results, num_executables, total_params);

    // You can use this to debug your scores function
    // get_score("results.txt", results[0].exe_path);

    // Print each score to scores.txt
    write_scores_to_file(results, num_executables, "results.txt");

    // TODO: Remove the message queue

    if (msqid != -1) {
        // Delete the message queue using msgctl()
        if (msgctl(msqid, IPC_RMID, NULL) == -1) { // remove message queue
            perror("msgctl");
            exit(1);
        }
    }


    // Free the results struct and its fields
    for (int i = 0; i < num_executables; i++) {
        free(results[i].exe_path);
        free(results[i].params_tested);
        free(results[i].status);
    }

    free(results);
    free(executable_paths);
    free(workers);
    
    return 0;
}