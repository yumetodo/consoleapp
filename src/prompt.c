#include "prompt.h"

const char DEFAULT_SC_HEAD[]       = {0x01, 0x00};
const char DEFAULT_SC_TAIL[]       = {0x05, 0x00};
const char DEFAULT_SC_NEXT_BLOCK[] = {0x1b, 0x5b, 0x31, 0x3b, 0x35, 0x43, 0x00};
const char DEFAULT_SC_PREV_BLOCK[] = {0x1b, 0x5b, 0x31, 0x3b, 0x35, 0x44, 0x00};
const char DEFAULT_SC_COMPLETION[] = {0x09, 0x00};
const char DEFAULT_SC_DIVE_HIST[]  = {0x1b, 0x5b, 0x41, 0x00};
const char DEFAULT_SC_FLOAT_HIST[] = {0x1b, 0x5b, 0x42, 0x00};
static const char right[]          = {0x1b, 0x5b, 0x43, 0x00};
static const char left[]           = {0x1b, 0x5b, 0x44, 0x00};
static const char delete[]         = {0x1b, 0x5b, 0x33, 0x7e, 0x1b, 0x00};

static char
getch(void)
{
    char   buf         = 0;
    struct termios old = {0};

    fflush(stdout);

    if(tcgetattr(0, &old)<0){
        perror("tcsetattr()");
    }

    old.c_lflag&=~ICANON;
    old.c_lflag&=~ECHO;
    old.c_cc[VMIN]=1;
    old.c_cc[VTIME]=0;

    if(tcsetattr(0, TCSANOW, &old)<0){
        perror("tcsetattr ICANON");
    }

    if(read(0,&buf,1)<0){
        perror("read()");
    }
    
    old.c_lflag|=ICANON;
    old.c_lflag|=ECHO;

    if(tcsetattr(0, TCSADRAIN, &old)<0){
        perror ("tcsetattr ~ICANON");
    }

    return buf;
}

static void
strndelete( /* NOTE: posの値がstrの範囲内にあるかの確認は呼び出しもとで行っているものとする */
        int    pos,
        char **str) /* [out] */
{
    if(*str == NULL){
        return;
    }

    int str_len = strlen(*str);

    if(str_len == 1){
        free(*str);
        *str = NULL;
        return;
    }
    (*str)[pos] = '\0';
    *str = (char *)realloc(*str, sizeof(char)*(str_len-1));
    if(str_len == pos+1){
        return;
    }
    sprintf(*str, "%s%s", *str, &(*str)[pos+1]);
}

static void
strninsert( /* NOTE: posの値がstrの範囲内にあるかの確認は呼び出しもとで行っているものとする */
        int    pos,
        char **str, /* [out] */
        char   ch)
{
    if(*str == NULL){
        *str = (char *)malloc(sizeof(char) * 1);
        (*str)[0] = ch;
        return;
    }

    int   str_len = strlen(*str);
    char *new     = (char *)malloc(sizeof(char)*(str_len+1));

    if(!new){
        free(*str);
        *str = NULL;
        return;
    }

    strcpy(new, *str);
    sprintf(&(new[pos]), "%c%s", ch, &((*str)[pos]));
    free(*str);
    *str = new;
}

sRwhCtx*
genRwhCtx(int history_size)
{
    sRwhCtx *ctx = NULL;

    if(!(ctx = (sRwhCtx*)malloc(sizeof(sRwhCtx)))){
        return NULL;
    }

    ctx -> history       = NULL;
    ctx -> sc_head       = NULL;
    ctx -> sc_tail       = NULL;
    ctx -> sc_next_block = NULL;
    ctx -> sc_prev_block = NULL;
    ctx -> sc_completion = NULL;
    ctx -> sc_dive_hist  = NULL;
    ctx -> sc_float_hist = NULL;

    if(!(ctx -> history = (sRingBuf *)malloc(sizeof(sRingBuf)))){
        goto free_and_exit;
    }
    ctx -> history -> buf        = NULL;
    ctx -> history -> size       = history_size;
    ctx -> history -> head       = 0;
    ctx -> history -> tail       = 0;
    ctx -> history -> entory_num = 0;

    if(!(ctx -> history -> buf = (char **)calloc(history_size, sizeof(char *)))){
        goto free_and_exit;
    }
    memset(ctx -> history -> buf, 0, history_size);

    if(!(ctx -> sc_head       = malloc(sizeof(char)*strlen(DEFAULT_SC_HEAD)))){
        goto free_and_exit;
    }

    if(!(ctx -> sc_tail       = malloc(sizeof(char)*strlen(DEFAULT_SC_TAIL)))){
        goto free_and_exit;
    }

    if(!(ctx -> sc_next_block = malloc(sizeof(char)*strlen(DEFAULT_SC_NEXT_BLOCK)))){
        goto free_and_exit;
    }

    if(!(ctx -> sc_prev_block = malloc(sizeof(char)*strlen(DEFAULT_SC_PREV_BLOCK)))){
        goto free_and_exit;
    }

    if(!(ctx -> sc_completion = malloc(sizeof(char)*strlen(DEFAULT_SC_COMPLETION)))){
        goto free_and_exit;
    }

    if(!(ctx -> sc_dive_hist  = malloc(sizeof(char)*strlen(DEFAULT_SC_DIVE_HIST)))){
        goto free_and_exit;
    }

    if(!(ctx -> sc_float_hist = malloc(sizeof(char)*strlen(DEFAULT_SC_FLOAT_HIST)))){
        goto free_and_exit;
    }

    strcpy(ctx -> sc_head,       DEFAULT_SC_HEAD);
    strcpy(ctx -> sc_tail,       DEFAULT_SC_TAIL);
    strcpy(ctx -> sc_next_block, DEFAULT_SC_NEXT_BLOCK);
    strcpy(ctx -> sc_prev_block, DEFAULT_SC_PREV_BLOCK);
    strcpy(ctx -> sc_completion, DEFAULT_SC_COMPLETION);
    strcpy(ctx -> sc_dive_hist,  DEFAULT_SC_DIVE_HIST);
    strcpy(ctx -> sc_float_hist, DEFAULT_SC_FLOAT_HIST);
    return ctx;

free_and_exit:
    free(ctx -> sc_float_hist);
    free(ctx -> sc_dive_hist);
    free(ctx -> sc_completion);
    free(ctx -> sc_next_block);
    free(ctx -> sc_prev_block);
    free(ctx -> sc_dive_hist);
    free(ctx -> sc_tail);
    free(ctx -> sc_head);
    if(!(ctx -> history)){
        free(ctx -> history -> buf);
    }
    free(ctx -> history);
    free(ctx);
    return NULL;
}

static void
push2Ringbuf(
        sRingBuf   *rb,
        char *str)
{
    /* buf is empty */
    if(rb->buf[rb->head] == NULL){
        rb -> buf[rb->head] = str;
        rb -> entory_num++;
    }
    /* buf is full */
    else if(rb->head == rb->tail+1 || rb->tail == rb->size-1){
        free(rb -> buf[rb -> head]);
        rb -> buf[rb -> head] = str;

        if(rb->head == rb->size-1){
            rb -> head = 0;
            rb -> tail++;
        }
        else if(rb->tail == rb->size-1){
            rb -> head = 1;
            rb -> tail = 0;
        }
        else{
            rb -> head++;
            rb -> tail++;
        }
    }
    /* buf is halfway */
    else{
        rb -> tail++;
        rb -> buf[rb -> tail] = str;
        rb -> entory_num++;
    }
}

static char *
readRingBuf(
        sRingBuf *rb,
        int depth)
{
    int idx = rb -> tail - depth;
    if(idx < 0){
        idx += rb -> size;
    }
    return rb -> buf[idx];
}

static int
judgeShortCut(
        sRwhCtx    *ctx,
        const char *str)
{
    /* return values */
    const int NOT_SHORT_CUT = 0;
    const int UNKNOWN_YET   = 1;
    const int SC_HEAD       = 2;
    const int SC_TAIL       = 3;
    const int SC_NEXT_BLOCK = 4;
    const int SC_PREV_BLOCK = 5;
    const int SC_COMPLETION = 6;
    const int SC_DIVE_HIST  = 7;
    const int SC_FLOAT_HIST = 8;
    const int RIGHT         = 9;  /* ショートカットでは無いがカーソル右が制御信号なので */ 
    const int LEFT          = 10; /* ショートカットでは無いがカーソル左が制御信号なので */ 
    const int DELETE        = 11; /* ショートカットでは無いがカーソル左が制御信号なので */ 

    /* flags */
    int sc_head_possibility       = 1;
    int sc_tail_possibility       = 1;
    int sc_next_block_possibility = 1;
    int sc_prev_block_possibility = 1;
    int sc_completion_possibility = 1;
    int sc_dive_hist_possibility  = 1;
    int sc_float_hist_possibility = 1;
    int right_possibility         = 1;
    int left_possibility          = 1;
    int delete_possibility        = 1;

    int str_len = strlen(str);

    for(int i=0; i<strlen(str); i++){
        int sc_len;
        if(sc_head_possibility){
            sc_len = strlen(ctx->sc_head);
            sc_head_possibility = sc_len >= str_len && ctx->sc_head[i] == str[i];
            if(sc_head_possibility && i == sc_len-1){
                return SC_HEAD;
            }
        }
        if(sc_tail_possibility){
            sc_len = strlen(ctx->sc_tail);
            sc_tail_possibility = sc_len >= str_len && ctx->sc_tail[i] == str[i];
            if(sc_tail_possibility && i == sc_len-1){
                return SC_TAIL;
            }
        }
        if(sc_next_block_possibility){
            sc_len = strlen(ctx->sc_next_block);
            sc_next_block_possibility = sc_len >= str_len && ctx->sc_next_block[i] == str[i];
            if(sc_next_block_possibility && i == sc_len-1){
                return SC_NEXT_BLOCK;
            }
        }
        if(sc_prev_block_possibility){
            sc_len = strlen(ctx->sc_prev_block);
            sc_prev_block_possibility = sc_len >= str_len && ctx->sc_prev_block[i] == str[i];
            if(sc_prev_block_possibility && i == sc_len-1){
                return SC_PREV_BLOCK;
            }
        }
        if(sc_completion_possibility){
            sc_len = strlen(ctx->sc_completion);
            sc_completion_possibility = sc_len >= str_len && ctx->sc_completion[i] == str[i];
            if(sc_completion_possibility && i == sc_len-1){
                return SC_COMPLETION;
            }
        }
        if(sc_dive_hist_possibility){
            sc_len = strlen(ctx->sc_dive_hist);
            sc_dive_hist_possibility = sc_len >= str_len && ctx->sc_dive_hist[i] == str[i];
            if(sc_dive_hist_possibility && i == sc_len-1){
                return SC_DIVE_HIST;
            }
        }
        if(sc_float_hist_possibility){
            sc_len = strlen(ctx->sc_float_hist);
            sc_float_hist_possibility = sc_len >= str_len && ctx->sc_float_hist[i] == str[i];
            if(sc_float_hist_possibility && i == sc_len-1){
                return SC_FLOAT_HIST;
            }
        }
        if(right_possibility){
            sc_len = strlen(right);
            right_possibility = sc_len >= str_len && right[i] == str[i];
            if(right_possibility && i == sc_len-1){
                return RIGHT;
            }
        }
        if(left_possibility){
            sc_len = strlen(left);
            left_possibility = sc_len >= str_len && left[i] == str[i];
            if(left_possibility && i == sc_len-1){
                return LEFT;
            }
        }
        if(delete_possibility){
            sc_len = strlen(delete);
            delete_possibility = sc_len >= str_len && delete[i] == str[i];
            if(delete_possibility && i == sc_len-1){
                return DELETE;
            }
        }
    }

    if(sc_head_possibility || sc_tail_possibility ||
    sc_next_block_possibility || sc_prev_block_possibility ||
    sc_completion_possibility || sc_dive_hist_possibility ||
    sc_float_hist_possibility || right_possibility ||
    left_possibility || delete_possibility)
    {
        return UNKNOWN_YET;
    }

    return NOT_SHORT_CUT;
}

static int
auxNextPrevBlock(
        int         incOrDec(int),
        int         curent_cursor_pos,
        const char *str)
{
    if(incOrDec(curent_cursor_pos) == 0){
        return 0;
    }

    if(str[incOrDec(curent_cursor_pos)] == '\0'){
        return curent_cursor_pos;
    }

    if(str[curent_cursor_pos] != ' ' && (str[curent_cursor_pos-1] == ' ' || str[curent_cursor_pos+1] == ' ')){
        return curent_cursor_pos;
    }

    return auxNextPrevBlock(incOrDec, incOrDec(curent_cursor_pos), str);
}

static int
nextBlock(
        int         curent_cursor_pos, 
        const char *str)
{
    if(str == NULL || curent_cursor_pos == strlen(str)){
        return curent_cursor_pos;
    }

    int inc(int n){
        return n + 1;
    }

    if(str[curent_cursor_pos-1] == ' ' || str[curent_cursor_pos+1] == ' '){
        curent_cursor_pos++;
    }

    return auxNextPrevBlock(inc, curent_cursor_pos, str);
}

static int
prevBlock(
        int curent_cursor_pos,
        const char *str)
{
    if(curent_cursor_pos == 0){
        return 0;
    }

    int dec(int n){
        return n - 1;
    }

    if(str[curent_cursor_pos-1] == ' ' || str[curent_cursor_pos+1] == ' '){
        curent_cursor_pos--;
    }

    return auxNextPrevBlock(dec, curent_cursor_pos, str);
}

static void clearLine(
        const char *prompt,
        const char *line)
{
    int line_len = line == NULL ? 0 : strlen(line);
    printf("\r");
    /* +1は直前の操作がbackspaceだった場合に, 1文字分lineからは消えているがコンソール上では消えていないため */
    for(int i=0; i<strlen(prompt)+line_len+1; i++){
        printf(" ");
    }
    printf("\r%s", prompt);
    fflush(stdout);
}

char *
rwh(
        sRwhCtx    *ctx,
        const char *prompt) /* in */
{
    char *line           = NULL;
    int   line_len       = 0;
    char *tmp            = NULL;
    int   tmp_len        = 0;
    int   cursor_pos     = 0;
    int   history_idx    = 0;
    char *evacated_line  = NULL;

    /* flags */
    char before_is_dive = 0;
    char dived          = 0;
    char line_modified  = 1;

    printf("%s", prompt);
    fflush(stdout);

    while(1){
        char ch = getch();
        switch(ch){
            case '\n':
                if(line){
                    push2Ringbuf(ctx->history, line);
                }
                printf("\n");
                return line == NULL ? "" : line;

            case 0x7f: /* backspace */
                if(cursor_pos != 0){
                    cursor_pos--;
                    line_len--;
                    strndelete(cursor_pos, &line);
                    line_modified = 1;
                }
                break;

            default:
                if(!(tmp = (char *)realloc(tmp, sizeof(char)*(tmp_len+1)))){
                    return NULL;
                }
                sprintf(tmp, "%s%c", tmp_len == 0 ? "" : tmp, ch);
                tmp_len++;
                switch(judgeShortCut(ctx, tmp)){
                    case 0: /* NOT_SHORT_CUT  */
                        strninsert(cursor_pos, &line, ch);
                        cursor_pos++;
                        line_len++;
                        line_modified = 1;
                        goto free_and_break;

                    case 1: /* UNKNOWN_YET */
                        /* nothing to do */
                        break;

                    case 2: /* SC_HEAD        */
                        cursor_pos = 0;
                        goto free_and_break;

                    case 3: /* SC_TAIL        */
                        cursor_pos = line_len;
                        goto free_and_break;

                    case 4: /* SC_NEXT_BLOCK  */
                        cursor_pos = nextBlock(cursor_pos, line);
                        goto free_and_break;

                    case 5: /* SC_PREV_BLOCK  */
                        cursor_pos = prevBlock(cursor_pos, line);
                        goto free_and_break;

                    case 6: /* SC_COMPLETION  */
                        goto free_and_break;

                    case 7: /* SC_DIVE_HIST   */
                        if(history_idx < ctx->history->entory_num){
                            before_is_dive = 1;
                            if(dived){
                                history_idx   += !before_is_dive;
                            }
                            if(line_modified){
                                evacated_line  = line;
                                line_modified  = 0;
                            }
                            dived          = 1;
                            clearLine(prompt, line);
                            line           = readRingBuf(ctx->history, history_idx++);
                            line_len       = strlen(line);
                            cursor_pos     = line_len;
                        }
                        goto free_and_break;

                    case 8: /* SC_FLOAT_HIST  */
                        if(dived){
                            history_idx   -= before_is_dive;
                            before_is_dive = 0;
                            clearLine(prompt, line);
                            if(history_idx == 0){
                                line = evacated_line;
                            }
                            else{
                                line = readRingBuf(ctx->history, --history_idx);
                            }
                            line_len   = line == NULL ? 0 : strlen(line);
                            cursor_pos = line_len;
                        }
                        goto free_and_break;

                    case 9: /* RIGHT  */
                        cursor_pos = cursor_pos == line_len ? cursor_pos : cursor_pos+1;
                        goto free_and_break;

                    case 10: /* LEFT  */
                        cursor_pos = cursor_pos == 0 ? 0 : cursor_pos-1;
                        goto free_and_break;

                    case 11: /* DELETE */
                        if(cursor_pos >= line_len){
                            strndelete(cursor_pos, &line);
                            line_len--;
                            line_modified = 1;
                        }
                        goto free_and_break;

                    default:
                        BUG_REPORT();
                        goto free_and_break;

                    free_and_break:
                        free(tmp);
                        tmp = NULL;
                        tmp_len = 0;
                        break;
                }
                break;
        }
        clearLine(prompt, line);
        printf("%s", line == NULL ? "" : line);
        for(int i=0; i<line_len-cursor_pos; i++){
            printf("\b");
        }
        fflush(stdout);
    }
}

void
freeRwhCtx(sRwhCtx *ctx){
    for(int i=0; i<ctx->history->size; i++){
        free(ctx -> history -> buf[i]);
    }
    free(ctx -> history -> buf);
    free(ctx -> history);
    free(ctx -> sc_head);
    free(ctx -> sc_tail);
    free(ctx -> sc_next_block);
    free(ctx -> sc_prev_block);
    free(ctx -> sc_completion);
    free(ctx -> sc_dive_hist);
    free(ctx -> sc_float_hist);
    free(ctx);
}

