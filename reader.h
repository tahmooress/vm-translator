#ifndef READER_H
#define READER_H
#include<stdio.h>

typedef struct Reader Reader;

Reader* reader_init(FILE* stream);
int reader_advance(Reader *r);
int reader_has_next(Reader *r);
char* reader_get_line(Reader *r);
void reset_reader(Reader *r);
size_t reader_current_line_number(Reader* r);

#endif
