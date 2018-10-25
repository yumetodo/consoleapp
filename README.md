# libconsoleapp.a
The libconsoleapp.a is a C library.
This library aim to provide general-purpose and flexible functions of the console application and to prepare for its development.  
Currently, this library can be useful for implementing the following two functions in the console app.  

1. Classification and error check of options received at startup.
2. Implementation of interactive function.

Hereafter, the function of 1 is called **consoleapp/option**, and the function of 2 is called **consoleapp/prompt**.  
**consoleapp/option** is implemented in option.c and option.h. **consoleapp/prompt** is implemented in prompt.c and prompt.h.

# consoleapp/option
**consoleapp/option** makes it easy to classification and error check of options received at startup.

## Struct reference

```c:option.h
/* structure for holding information on each option specified at program execution */
typedef struct _opt_group_t{
    unsigned int priority;    /* a unique number corresponding to option */
    int          content_num; /* number of elements of content */
    char       **contents;    /* content attached to options. for example, For example, 1 and 2 in "somecommand - foo = 1, 2 bar" are assigned to the contents [0] and contents [1] of the foo option. */
}opt_group_t;
```

## Function reference
```c:option.h
extern int /* OPTION_SUCCESS or OPTION_FAILURE */
regOptProperty( /*  */
        unsigned int priority,        /* order of options to be extracted by popOptGroup (priority) */
        const char *short_form,      /* [in] short format of option, for example, -h. */
        const char *long_form,       /* [in] long format of option. for example, --help. */
        int          content_num_min, /* minimum number of contents attached to option */
        int          content_num_max, /* maximum number of contents attached to option */
        int        (*contentsChecker)(char **contents, int content_num)); /* callback function to check option contents */
```

```c:option.h
extern int /* OPTION_SUCCESS or OPTION_FAILURE */
groupingOpt( /* function which grouping options by option information registered in regOptionProperty and arguments of main obtained from cli. */
        int     argc,        /* first argument of main */
        char   *argv[],      /* second argument of main */
        int    *optless_num, /* [out] strings of argv which not belonging to any option */
        char ***optless);    /* [out] number of elements of optless_num */
```

```c:option.h
extern opt_group_t* /* a pointer to opt_group_t generated by groupingOpt, or NULL if it exceeds size. */
popOptGroup(void); /* a function that returns a pointer to opt_group_t generated by groupingOpt The order of the pointers to return opt_group_t depends on the priority registered in regOptionProperty. */
```

```c:option.h
extern int  /* the value of the error code. When the first OPTION_SUCCESS is returned, all error codes obtained by this function become OPTION_SUCCESS. */
popOptErrcode(void); /* a function that returns the error code obtained by adapting the contentsChecker registered with regOptionProperty to each option.The order of return depends on the priority registered in regOptionProperty. */
```

```c:option.h
extern void
endOptAnalization(void); /* function to release all dynamic memory secured by consoleapp/option */
```

## Sample code
This is a part of "sample/sample.c". The flow of the Program is,

1. register option properties by regOptProperty().
2. search and classify the strings received from cli for which option belongs to which option.
3. check error information generated during option analysis.
4. if there is no error in 3, the process corresponding to the option is executed.

```c:sample.c
int main(int argc, char *argv[]){

    /* here!!! 1 */
    regOptProperty(HELP,        "-h", "--help",        0, 0,       NULL);
    regOptProperty(VERSION,     "-v", "--version",     0, 0,       NULL);
    regOptProperty(PRINT,       "-p", "--print",       1, INT_MAX, NULL);
    regOptProperty(INTERACTIVE, "-i", "--interactive", 1, 1,       chkOptInteractive);

    /* here!!! 2 */
    int    optless_num = 0;
    char **optless     = NULL;
    if(groupingOpt(argc, argv, &optless_num, &optless) == OPTION_FAILURE){
        fprintf(stderr, "sample: an error occurred while parsing options.\n");
        return 0;
    }

    /* here!!! 3 */
    int errcode = OPTION_SUCCESS;
    while((errcode = popOptErrcode()) != OPTION_SUCCESS){
        switch(errcode){
            case INTERACTIVE_ILLIGAL_NUMBER:
                fprintf(stderr, "error: the history size specified with the option -i(--interactive) is an invalid value\n");
                exit(1);
        }
    }

    /* here!!! 4 */
    opt_group_t *opt_grp_p = NULL;
    while((opt_grp_p = popOptGroup()) != NULL){
        switch(opt_grp_p -> priority){
            case HELP:
                printUsage();
                break;

            case VERSION:
                printVersion();
                break;

            case PRINT:
                for(int i=0; i<opt_grp_p->content_num; i++){
                    printf("%s\n", opt_grp_p->contents[i]);
                }
                break;

            case INTERACTIVE:
                interactive(atoi(opt_grp_p->contents[0]));
                break;

            default:
                fprintf(stderr, "error: there is a bug at %d in %s\n", __LINE__, __FILE__);
                break;
        }
    }

    if(optless != NULL){
        printf("these optionless contents are ignored in this sample.\n");
        for(int i=0; i<optless_num; i++){
            printf("%s\n", optless[i]);
        }
    }

    return 0;
}
```

```c:sample.c
/* here!!! 3 */
int chkOptInteractive(char **contents, int dont_care){
    const int SUCCESS        = 0;
    const int ILLIGAL_NUMBER = 1;

    int num = atoi(contents[0]);
    if(num < 1){
        return ILLIGAL_NUMBER;
    }
    return SUCCESS;
}
```
## Demo
![option_demo](doc/option_demo.gif)

# consoleapp/prompt
**consoleapp/prompt** makes it easy to implementation of interactive function.

## Struct reference
The most important structure is rwhctx_t, and structures other than rwhctx_t are defined for rwhctx_t.

```c:prompt.h
/* structure for ring buffer. this is used for rwh_ctx_t's member. there is no need for user to know. */
typedef struct _ringbuf_t{
    char **buf;         /* buffer for entories */
    int    size;        /* max size of buffer */
    int    head;        /* buffer index at an oldest entory */
    int    tail;        /* buffer index at an newest entory */
    int    entory_num;  /* number of entories */
}ringbuf_t;
```

```c:prompt.h
/* structure for holding candidates at completion. */
typedef struct _completion_t{
    char** entories;   /* entories are sorted in ascending order */
    int    entory_num; /* number of entories */
}completion_t;
```

```c:primpt.h
/* structure for preserve context for rwh(). */
typedef struct _rwhctx_t{
    const char   *prompt;        /* prompt */
    ringbuf_t    *history;       /* history of lines enterd in the console */
    completion_t *candidate;     /* search target at completion */
    char         *sc_head;       /* shortcut for go to the head of the line */
    char         *sc_tail;       /* shortcut for go to the tail of the line */
    char         *sc_next_block; /* shortcut for go to the next edge of the word of the line */
    char         *sc_prev_block; /* shortcut for go to the previous edge of the word of the line */
    char         *sc_completion; /* shortcut for completion */
    char         *sc_dive_hist;  /* shortcut for fetch older history */
    char         *sc_float_hist; /* shortcut for fetch newer history */
}rwhctx_t;
```

## Function reference
The most important function is rwh(), and functions other than rwh() are defined for rwh().  
Incidentary, rwh is short form of Readline With History. However, complementary function was added in the development process.

```c:prompt.h
extern completion_t* /* NULL if fails */
genCompletion( /* generate a completion_t */
        const char **strings,      /* [in] search target at completion */
        int          string_num);  /* number of candidate */
```

```c:prompt.h
extern rwhctx_t* /* a generated rwh_ctx_t pointer which shortcut setting fields are set to default. if failed, it will be NULL. */
genRwhCtx( /* generate a rwh_ctx_t pointer. */
        const char  *prompt,         /* [in] prompt */
              int    history_size,   /* max size of the buffer of the history */
        const char **candidates,     /* [in] search target at completion */ 
              int    candidate_num); /* number of candidates */
```

```c:prompt.h
extern char * /* enterd line */
rwh( /* acquire the line entered in the console and keep history. */
        rwhctx_t   *ctx);      /* [mod] an context generated by genRwhCtx(). ctx keeps shortcuts and history operation keys settings and history. after rwh(), the entories of history of ctx is updated. */
```

```c:prompt.h
extern void
freeRwhCtx( /* free rwhctx_t pointer recursively. */
        rwhctx_t *ctx); /* [mod] to be freed */
```


## Sample code
This is a part of "sample/sample.c". The flow of the Program is,

1. make context of rwhctx\_t for mode1.
2. make context of rwhctx\_t for mode2.
3. get line from prompt by rwh() with current mode context.
4. Perform processing corresponding to the content of acquired line.

```c
void interactive(int hist_entory_size){
    char     *line;
    int       mode = 1;

    const char *commands1[] = {"help", "quit", "ctx", "modctx", "!echo", "!ls", "!pwd", "!date", "!ls -a", "!ls -l", "!ls -lt"};
    const char *commands2[] = {"help", "ctx", "head", "tail", "next block", "prev block", "completion", "dive hist", "float hist", "done"};

    /* here!!! 1 */
    rwhctx_t *ctx1 = genRwhCtx("sample$ "       , hist_entory_size, commands1, sizeof(commands1)/sizeof(char *));
    /* here!!! 2 */
    rwhctx_t *ctx2 = genRwhCtx("modctx@sample$ ", hist_entory_size, commands2, sizeof(commands2)/sizeof(char *));

    printf("input \"help\" to display help\n");

    while(1){
        switch(mode){
            case 1:
                /* here!!! 3 */
                line = rwh(ctx1);

                /* here!!! 4 */
                if(strcmp(line, "help") == 0){
                    interactiveHelp1();
                }
                else if(strcmp(line, "ctx") == 0){
                    interactivePrintCtx(ctx1);
                }
                ** Abb **
                else{
                    printf("err\n");
                }
                break;

            case 2:
                /* here!!! 3 */
                line = rwh(ctx2);

                /* here!!! 4 */
                if(strcmp(line, "help") == 0){
                    interactiveHelp2();
                }
                else if(strcmp(line, "ctx") == 0){
                    interactivePrintCtx(ctx2);
                }
                ** Abb **
                else{
                    printf("err\n");
                }
                break;
        }
    }

free_and_exit:
    freeRwhCtx(ctx1);
    freeRwhCtx(ctx2);
}
```

## Demo
![option_demo](doc/prompt_demo.gif)

## Installation

Watch [doc/INSTALL.md](doc/INSTALL.md)

## Documentation
### Todo
I've written or not written in [doc/TODO.md](doc/TODO.md), bugs, idea, and anything else that I can not remember.

### Contoributer
Contributor is managed with [doc/CONTRIBUTER.md](doc/CONTRIBUTER.md). Please describe it freely in this file, if you commit.

### Copyright
We are proceeding with development under the MIT license. When you want to use the deliverables of this project, you can use it without permission. For details, please see the [doc/COPYRIGHT](doc/COPYRIGHT).
