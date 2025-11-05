#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <ctype.h>

#define MAX_LINE_SIZE 512

typedef struct Reader{
    FILE* stream;
    char line_buffer[MAX_LINE_SIZE];
    char* line;
    size_t line_number;
    int hasNext;
} Reader;

static char* trim(char* s);

Reader* reader_init(FILE* stream)
{
    if (!stream) return NULL;

    Reader* r = (Reader*) malloc(sizeof(Reader));
    r->stream = stream;
    r->hasNext = 1;
    r->line_number = 0;
    
    return r;
}

int reader_advance(Reader *r)
{   
    char* line;
    while(fgets(r->line_buffer, MAX_LINE_SIZE, r->stream) != NULL) {
        line = trim(r->line_buffer);
        if (line && *line) {
            r->hasNext = 1;
            r->line_number++;
            r->line = line;
            return 0;
        }
    }

    r->hasNext = 0;
    return EOF;
}

int reader_has_next(Reader *r)
{
    return r->hasNext;
}

char* reader_get_line(Reader *r)
{
    return r->line;
}

size_t reader_current_line_number(Reader* r)
{
    return r->line_number;
}

void reset_reader(Reader *r)
{   
    if (!r) return;

    if (r->stream) 
    {
        fseek(r->stream, 0, SEEK_SET);
    }

    r->hasNext = 1;
    r->line_number = 0;
    
    for (size_t i = 0; i < MAX_LINE_SIZE; i++) {
        r->line_buffer[i] = 0;
    }
}

static char* trim(char* s) {
    if (!s) return NULL;

    while (isspace((unsigned char)*s)) s++;
    if (*s == 0) return s;

   
    char* end;
    if ((end = strstr(s, "//")) != NULL) {
        end--;
    }else {
        end = s + strlen(s) - 1;
    }

    while (end > s && (isspace((unsigned char)*end))) end--;

    end[1] = '\0';

    return s;
}
