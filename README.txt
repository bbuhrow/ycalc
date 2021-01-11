Implements an extendable arbitrary precision calculator.
Supports function calls with optional arguments.
Supports interactive use by keeping internal state (user variables).
Depends on GMP for arbitrary arithmetic.

Library Usage:
    calc_init();                                        // set up some internal storage
    process_expression(expression_string, metadata);    // process expression_string
    calc_finalize();                                    // free internal storage

Demo program usage:
    1) If no arguments on command line, enter an interactive session.
    2) If string arguments on the command line, run calc on them and return.
    3) If redirect or pipe is detected, ignore any command line arguments, run
        calc on the lines in the file, and return;
    