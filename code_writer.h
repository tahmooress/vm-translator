#ifndef CODE_WRITER_H
#define CODE_WRITER_H
#include"./parser.h"
#endif

#pragma once

typedef enum {
    VM_ANNOTATE = 1u << 0,
    VM_DEBUG    = 1u << 1,
} Flags;

typedef struct CodeWriter CodeWriter;

CodeWriter* init_code_writer(FILE * out, const char* filename,Flags flags);
void write_halt(FILE* out);
void segment_initializer(CodeWriter* code_writer);
int write_code(CodeWriter* code_writer, Statement* stm);
void free_code_writer(CodeWriter* code_writer);