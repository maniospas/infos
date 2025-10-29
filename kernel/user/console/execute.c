#include "../console.h"

#define TMP_SIZE APPLICATION_MESSAGE_SIZE

static int isspace_local(unsigned char c) {
    return c==' ' || c=='\t'|| c=='\n' || c=='\r' || c=='\v' || c=='\f';
}

static inline void replace_segment(
    char *buf, 
    size_t *len, 
    size_t start, 
    size_t end,
    const char *replacement, 
    size_t repl_len
) {
    if(end > *len) end = *len;
    if (start > end) 
        return;

    // --- New: Add space padding if missing ---
    int add_space_before = 0;
    int add_space_after = 0;
    if(start > 0 && !isspace_local((unsigned char)buf[start - 1]))
        add_space_before = 1;
    if(end < *len && !isspace_local((unsigned char)buf[end]))
        add_space_after = 1;

    // adjust replacement with extra spaces if needed
    char temp[TMP_SIZE];
    size_t new_repl_len = repl_len + add_space_before + add_space_after;
    if (new_repl_len >= TMP_SIZE)
        new_repl_len = TMP_SIZE - 1;

    size_t pos = 0;
    if(add_space_before)
        temp[pos++] = ' ';
    memcpy(temp + pos, replacement, repl_len);
    pos += repl_len;
    if(add_space_after)
        temp[pos++] = ' ';
    temp[pos] = '\0';
    new_repl_len = pos;
    // ----------------------------------------

    size_t suffix_len = *len - end;
    if (start + new_repl_len + suffix_len >= TMP_SIZE)
        new_repl_len = TMP_SIZE - start - suffix_len - 1;

    memmove(buf + start + new_repl_len, buf + end, suffix_len + 1);
    memcpy(buf + start, temp, new_repl_len);
    *len = start + new_repl_len + suffix_len;
    buf[*len] = '\0';
}

extern uint32_t focus_id;

int console_execute(Application *app) {

    if (focus_id!=0 && app==&apps[0]) { // the console may send stuff to other apps
        // Check if user wants to bypass focus
        if (*app->data == ':') {
            app->data[0] = ' ';
        } 
        else {
            char* cmd = app->data;
            // Forward command to focused app
            if (focus_id < MAX_APPLICATIONS && apps[focus_id].run && apps[focus_id].window) {
                size_t len = strlen(cmd);
                if (len >= APPLICATION_MESSAGE_SIZE)
                    len = APPLICATION_MESSAGE_SIZE - 1;
                memcpy(apps[focus_id].input, cmd, len);
                apps[focus_id].input[len] = '\0';
                apps[focus_id].input_state = len;
                fb_write_ansi(app->window, "\x1b[32mOK\x1b[0m Message sent to: app");
                fb_write_dec(app->window, focus_id);
                fb_write(app->window, "\n");
                return CONSOLE_EXECUTE_OK;
            } else {
                fb_write_ansi(app->window, "\x1b[31mERROR\x1b[0m Focused app no longer exists.\n");
                focus_id = 0;
                return CONSOLE_EXECUTE_RUNTIME_ERROR;
            }
        }
    }

    char *buffer = malloc(TMP_SIZE);
    if(!buffer)
        return CONSOLE_EXECUTE_OOM;
    char *inner = malloc(TMP_SIZE);
    if(!inner) {
        free(buffer);
        return CONSOLE_EXECUTE_OOM;
    }
    char *output = malloc(TMP_SIZE);
    if(!output) {
        free(buffer);
        free(inner);
        return CONSOLE_EXECUTE_RUNTIME_ERROR;
    }

    char *saved_data = app->data;
    char *saved_output = app->output;

    strncpy(buffer, app->data, TMP_SIZE - 1);
    buffer[TMP_SIZE - 1] = 0;
    size_t len = strlen(buffer);

    for(;;) {
        int brace_depth = 0;
        int paren_depth = 0;
        size_t start = 0;
        size_t end = 0;

        for (size_t i = 0; i < len; i++) {
            char c = buffer[i];
            if(c == '{') {
                brace_depth++;
                continue;
            } 
            else if(c == '}') {
                if (brace_depth > 0) 
                    brace_depth--;
                continue;
            }
            if(brace_depth) 
                continue;
            if(c == '(') {
                paren_depth++;
                start = i;
                end = 0;
            } 
            else if(c == ')') {
                end = i;
                paren_depth--;
                break; // break immediately - eagerly evaluate first inner parenthesis
            } 
            else if(c==':') {
                paren_depth++;
                start = i;
                end = len;
            }
        }

        if(!end) 
            break;

        // find inner expression
        size_t inner_start = start + 1;
        size_t inner_end = end;
        size_t inner_len = inner_end - inner_start;
        if (inner_len >= TMP_SIZE) 
            inner_len = TMP_SIZE - 1;
        memcpy(inner, buffer+inner_start, inner_len);
        inner[inner_len] = 0;

        // run inner expression on temporary output
        app->data = inner;
        app->output = output;
        app->output[0] = 0;
        app->output_state = 0;
        int ret = console_command(app);
        if(ret) {
            app->data = saved_data;
            app->output = saved_output;
            app->output[0] = 0;
            app->output_state = 0;
            free(buffer);
            free(output);
            free(inner);
            return ret;
        }

        // replace (inner) with evaluated output
        size_t repl_len = strlen(output);
        replace_segment(buffer, &len, start, end + 1, output, repl_len);
    }

    // Remove top-level { ... } blocks
    size_t write_pos = 0;
    int depth = 0;
    for (size_t i = 0; i < len; i++) {
        char c = buffer[i];
        if(c == '{') {
            if (depth == 0) { 
                depth = 1; 
                continue; 
            }
            depth++;
        } 
        else if(c == '}') {
            if(depth > 0) {
                depth--;
                if(depth == 0) 
                    continue;
            }
        }
        buffer[write_pos++] = c;
    }
    buffer[write_pos] = '\0';
    len = write_pos;

    strncpy((char*)app->data, buffer, len+1); // include the null termination

    // cleanup and run final command 
    free(inner);
    free(output);
    app->data = buffer;
    app->output = saved_output;
    app->output[0] = 0;
    app->output_state = 0;
    int ret = console_command(app);
    app->data = saved_data;
    free(buffer);
    return ret;
}
