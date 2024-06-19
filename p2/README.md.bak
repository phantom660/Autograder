How to compile code: 
    i.   On command line, navigate (cd) into the directory p2
    ii.  On the command line, run make <compile option> N=<Number of executables> (This creates the test folder with N executables and also generates an
         executable for the autograder) (compile option is exec and redir for intermediate)
    iii. Run autograder with the command ./autograder <solutions dir> arg1 arg2 arg3 ... (where B is the batch size, arg1, arg2, etc. are the command line
         arguments that each executale is run with) (The output will be printed in a file called "results.txt")

    eg.
    $ make redir N=5
    $ ./autograder solutions 1 2 3

Assumptions:
    i.    For stuck/infloop case, I killed the process and updated the child_status to 2 in the timeout_handler(). In monitor_and_evaluate_solutions(), if
          child_status is 2, I give it case 4 and set child_status to -1 to indicate the process is not running anymore
    ii.   We have a different way of handling stuck/infloop case as above (I don't know if it's the best way to do it, but we use alarm and but it works)
    iii.  There's a bunch of commented out code (especially in message queue (worker.c and mq_autograder.c)) that were used for debugging. We haven't removed
          those
    iv.   We havent removed the TODOs from the file either

Team info: Group 18
    Members: Tamojit Bera (bera0041)
             Nikhil Kumar Cherukuri (cheru054)
             Pratham Khandelwal (khand113)
             Merrick McFarling (mcfar288)

Contributions: Tamojit (bera0041):
                    Worked on autograder.c, template.c, utils.c, mq_autograder.c, and worker.c (most of mqueue implemetation)
               Nikhil (cheru054):
                    Worked on get_batch_size function, was asked to work on message queue and tried working on it, but unable to finish it
               Pratham (khand113):
                    Helped set up message queue, worked on utils.c, template.c, get_scores, get_batch_size. Commented the 
                    code and worked on readme file
               Merrick (mcfar288):
                    Set up git for the team, and reserved spot in office hours queue for the team. Was 
                    asked to work on get_batch_size function and on message queue but never got to them