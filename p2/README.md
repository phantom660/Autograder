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
