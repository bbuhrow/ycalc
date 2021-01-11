/*
MIT License

Copyright (c) 2021 Ben Buhrow

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 */

 /*
     A demo program utilizing ycalc library.
     If no arguments on command line, enter an interactive session.
     If string arguments on the command line, run calc on them and return.
     If redirect or pipe is detected, ignore any command line arguments, run
        calc on the lines in the file, and return;

 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h> //isatty
#include "calc.h"

#if defined(__unix__)
#include <termios.h>
#define CMDHIST_SIZE 16
static char** CMDHIST;
static int CMDHIST_HEAD = 0;
static int CMDHIST_TAIL = 0;
#endif

int exp_is_open(char* line, int firstline);
char* get_input(char* input_exp, uint32_t* insize);
char* process_batchline(int* code);

// gcc -O2  calc.h calc.c demo.c -o demo  -lgmp -lm


int main(int argc, char** argv)
{
    int have_redirect = 0;

#if defined(__MINGW32__)
    // I'm not sure how to detect at runtime if this is an msys shell.
    // So unfortunately if we compile with mingw we basically have to remove 
    // the ability to process from pipes or redirects.  should be able to use 
    // batchfiles via command line switch still.
    if (0)
    {

#elif defined(WIN32) 
    if (_isatty(_fileno(stdin)) == 0)
    {
        fseek(stdin, -1, SEEK_END);
        if (ftell(stdin) >= 0)
        {
            rewind(stdin);
#else
    if (isatty(fileno(stdin)) == 0)
    {
#endif

        // ok, we have incoming data in a pipe or redirect
        have_redirect = 1;
    }
#if defined(WIN32) && !defined(__MINGW32__)		// not complete, but ok for now
        }
#endif

    calc_init();

    if ((argc == 1) && (have_redirect == 0))
    {
        // with no arguments to the program, enter an interactive loop
        // to process commands
        uint32_t insize = GSTR_MAXSIZE;
        char* input_exp, * ptr, * indup, * input_line;
        str_t input_str;
        int firstline = 1;

        //the input expression
        input_exp = (char*)malloc(GSTR_MAXSIZE * sizeof(char));
        indup = (char*)malloc(GSTR_MAXSIZE * sizeof(char));
        input_line = (char*)malloc(GSTR_MAXSIZE * sizeof(char));
        sInit(&input_str);
        strcpy(input_exp, "");
        strcpy(input_line, "");
        printf(">> ");

#if defined(__unix__)
        int i;

        static struct termios oldtio, newtio;
        tcgetattr(0, &oldtio);
        newtio = oldtio;
        newtio.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK);

        CMDHIST = (char**)malloc(CMDHIST_SIZE * sizeof(char*));
        for (i = 0; i < CMDHIST_SIZE; i++)
        {
            CMDHIST[i] = (char*)malloc(GSTR_MAXSIZE * sizeof(char));
        }

#endif

        while (1)
        {

            {
#if defined(__unix__)
                tcsetattr(0, TCSANOW, &newtio);
#endif
                input_line = get_input(input_line, &insize);
#if defined(__unix__)
                tcsetattr(0, TCSANOW, &oldtio);
#endif
            }

            // exit, or execute the current expression...
            if ((strcmp(input_line, "quit") == 0) || (strcmp(input_line, "exit") == 0))
            {
                break;
            }
            else
            {
                sAppend(input_line, &input_str);
                if (exp_is_open(input_line, firstline))
                {
                    if (strlen(input_line) > 0)
                        sAppend(",", &input_str);
                    firstline = 0;
                    continue;
                }

                firstline = 1;
                process_expression(input_str.s, NULL);
                sClear(&input_str);
            }

#if defined(WIN32) && !defined(__MINGW32__)
            fflush(stdin);	//important!  otherwise scanf will process printf's output

#else

            fflush(stdin);	//important!  otherwise scanf will process printf's output
            fflush(stdout);

#endif

            // re-display the command prompt
            printf(">> ");
            fflush(stdout);

        }

        free(input_exp);
        free(input_line);
        free(indup);
        sFree(&input_str);

#if defined(__unix__)
        for (i = 0; i < CMDHIST_SIZE; i++)
        {
            free(CMDHIST[i]);
        }
        free(CMDHIST);
#endif

    }
    else if (have_redirect)
    {
        uint32_t insize = GSTR_MAXSIZE;
        char *input_line;
        str_t input_str;
        int firstline = 1;

        //the input expression
        sInit(&input_str);

        while (1)
        {
            int code;

            input_line = process_batchline(&code);

            if (code == 1)
            {
                break;
            }
            else if (code == 2)
            {
                continue;
            }

            // exit, or execute the current expression...
            if ((strcmp(input_line, "quit") == 0) || (strcmp(input_line, "exit") == 0))
            {
                free(input_line);
                break;
            }
            else
            {
                sAppend(input_line, &input_str);
                if (exp_is_open(input_line, firstline))
                {
                    if (strlen(input_line) > 0)
                        sAppend(",", &input_str);
                    firstline = 0;
                    free(input_line);
                    continue;
                }

                free(input_line);
                firstline = 1;
                process_expression(input_str.s, NULL);
                sClear(&input_str);
            }
        }

        sFree(&input_str);
    }
    else
    {
        // else we expect expressions on the command line to process
        int i;
        for (i = 1; i < argc; i++)
        {
            process_expression(argv[i], NULL);
        }
    }
    
    calc_finalize();

    return 0;
}


int exp_is_open(char* line, int firstline)
{
    int i;
    static int openp, closedp, openb, closedb;

    if (firstline)
    {
        openp = openb = closedp = closedb = 0;
    }

    for (i = 0; i < strlen(line); i++)
    {
        if (line[i] == '(') openp++;
        if (line[i] == ')') closedp++;
        if (line[i] == '{') openb++;
        if (line[i] == '}') closedb++;
    }
    if ((openp == closedp) && (openb == closedb))
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

char* get_input(char* input_exp, uint32_t* insize)
{
#if !defined(__unix__)

    // get command from user
    fgets(input_exp, GSTR_MAXSIZE, stdin);

    while (1)
    {
        if (input_exp[strlen(input_exp) - 1] == 13 || input_exp[strlen(input_exp) - 1] == 10)
        {
            // replace with a null char and continue
            printf("\n");
            fflush(stdout);
            input_exp[strlen(input_exp) - 1] = '\0';
            break;
        }
        else
        {
            // last char is not a carriage return means
            // the input is longer than allocated.
            // reallocate and get another chunk
            *insize += GSTR_MAXSIZE;
            input_exp = (char*)realloc(input_exp, *insize * sizeof(char));
            if (input_exp == NULL)
            {
                printf("couldn't reallocate string when parsing\n");
                exit(-1);
            }
            fgets(input_exp + strlen(input_exp), GSTR_MAXSIZE, stdin);
        }
    }

#else

    int n = 0;
    int p = CMDHIST_HEAD;
    strcpy(CMDHIST[p], "");

    while (1)
    {
        int c = getc(stdin);

        // is this an escape sequence?
        if (c == 27) {
            // "throw away" next two characters which specify escape sequence
            int c1 = getc(stdin);
            int c2 = getc(stdin);
            int i;

            if ((c1 == 91) && (c2 == 65))
            {
                // clear the current screen contents
                for (i = 0; i < n; i++)
                    printf("\b");

                for (i = 0; i < n; i++)
                    printf(" ");

                for (i = 0; i < n; i++)
                    printf("\b");

                // save whatever is currently entered                
                if (p == CMDHIST_HEAD)
                {
                    input_exp[n] = '\0';
                    memcpy(CMDHIST[CMDHIST_HEAD], input_exp, GSTR_MAXSIZE * sizeof(char));
                }

                // uparrow     
                if (CMDHIST_HEAD >= CMDHIST_TAIL)
                {
                    p--;

                    // wrap
                    if (p < 0)
                    {
                        // but not past tail
                        p = 0;
                    }
                }
                else
                {
                    p--;

                    // wrap
                    if (p < 0)
                    {
                        p = CMDHIST_SIZE - 1;
                    }
                }

                // and print the previous one
                printf("%s", CMDHIST[p]);
                strcpy(input_exp, CMDHIST[p]);
                n = strlen(input_exp);
            }
            else if ((c1 == 91) && (c2 == 66))
            {
                // downarrow
                // clear the current screen contents
                for (i = 0; i < strlen(CMDHIST[p]); i++)
                    printf("\b");

                for (i = 0; i < strlen(CMDHIST[p]); i++)
                    printf(" ");

                for (i = 0; i < strlen(CMDHIST[p]); i++)
                    printf("\b");

                if (p != CMDHIST_HEAD)
                {
                    p++;

                    // wrap
                    if (p == CMDHIST_SIZE)
                    {
                        p = 0;
                    }
                }

                // and print the next one
                printf("%s", CMDHIST[p]);
                strcpy(input_exp, CMDHIST[p]);
                n = strlen(input_exp);
            }
            else if ((c1 == 91) && (c2 == 67))
            {
                // rightarrow
            }
            else if ((c1 == 91) && (c2 == 68))
            {
                // leftarrow
            }
            else
            {
                printf("unknown escape sequence %d %d\n", c1, c2);
            }

            continue;
        }

        // if backspace
        if (c == 0x7f)
        {
            //fprintf(stderr,"saw a backspace\n"); fflush(stderr);
            if (n > 0)
            {
                // go one char left
                printf("\b");
                // overwrite the char with whitespace
                printf(" ");
                // go back to "now removed char position"
                printf("\b");
                n--;
            }
            continue;
        }

        if (c == EOF)
        {
            printf("\n");
            exit(0);
        }

        if ((c == 13) || (c == 10))
        {
            input_exp[n++] = '\0';
            break;
        }

        putc(c, stdout);
        input_exp[n++] = (char)c;

        if (n >= *insize)
        {
            *insize += GSTR_MAXSIZE;
            input_exp = (char*)realloc(input_exp, *insize * sizeof(char));
            if (input_exp == NULL)
            {
                printf("couldn't reallocate string when parsing\n");
                exit(-1);
            }
        }
    }

    printf("\n");
    fflush(stdout);

    if (strlen(input_exp) > 0)
    {
        memcpy(CMDHIST[CMDHIST_HEAD++], input_exp, GSTR_MAXSIZE * sizeof(char));

        if (CMDHIST_TAIL > 0)
        {
            CMDHIST_TAIL++;
            if (CMDHIST_TAIL == CMDHIST_SIZE)
                CMDHIST_TAIL = 0;
        }

        if (CMDHIST_HEAD == CMDHIST_SIZE)
        {
            CMDHIST_HEAD = 0;
            if (CMDHIST_TAIL == 0)
                CMDHIST_TAIL = 1;
        }
    }

#endif

    return input_exp;
}

char* process_batchline(int* code)
{
    int nChars, j, i;
    char* line, tmpline[GSTR_MAXSIZE], * ptr, * ptr2;
    FILE* batchfile, * tmpfile;

    batchfile = stdin;

    // load the next line of the batch file and get the expression
    // ready for processing
    line = (char*)malloc(GSTR_MAXSIZE * sizeof(char));
    strcpy(line, "");

    // read a line - skipping blank lines
    do
    {
        while (1)
        {
            ptr = fgets(tmpline, GSTR_MAXSIZE, batchfile);
            strcpy(line + strlen(line), tmpline);

            // stop if we didn't read anything
            if (feof(batchfile))
            {
                printf("eof; done processing batchfile\n");
                fclose(batchfile);
                *code = 1;
                free(line);
                return NULL;
            }

            if (ptr == NULL)
            {
                printf("fgets returned null; done processing batchfile\n");
                fclose(batchfile);
                *code = 1;
                free(line);
                return NULL;
            }

            // if we got the end of the line, stop reading
            if ((line[strlen(line) - 1] == 0xa) ||
                (line[strlen(line) - 1] == 0xd))
                break;

            // else reallocate the buffer and get some more
            line = (char*)realloc(line, (strlen(line) + GSTR_MAXSIZE) * sizeof(char));
        }

        // remove LF an CRs from line
        nChars = 0;
        for (j = 0; j < strlen(line); j++)
        {
            switch (line[j])
            {
            case 13:
            case 10:
                break;
            default:
                line[nChars++] = line[j];
                break;
            }
        }
        line[nChars++] = '\0';

    } while (strlen(line) == 0);

    //ignore blank lines
    if (strlen(line) == 0)
    {
        *code = 2;
        free(line);
        return NULL;
    }

    //ignore comment lines
    if (((line[0] == '/') && (line[1] == '/')) || (line[0] == '%'))
    {
        *code = 2;
        free(line);
        return NULL;
    }

    *code = 0;
    return line;
}
