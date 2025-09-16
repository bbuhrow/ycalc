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

Algorithm details:
1st tokenizes the string.
a token in this context is one of the following things:
  a number, possibly including a base prefix (0x, 0d, 0b, etc)
  a variable name
  a function name
  an operator string (includes parens, commas)

Then runs Dijkstra's shunting algorithm to create a postfix expression from the tokens.

Finally evaluates the postfix expression.
