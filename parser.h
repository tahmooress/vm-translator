#ifndef PARSER_H
#define PARSER_H
#include<stdio.h>
#include"./table.h"

#define C_LCL "local"
#define C_ARG "argument"
#define C_THIS "this"
#define C_THAT "that"
#define C_POINTER "POINTER"
#define C_TMP "TEMP"
#define C_STATIC "static"
#define C_CONSTANT "constant"

typedef enum {
    C_ARITHMETIC,
    C_PUSH,
    C_POP,
    C_LABEL,
    C_GOTO,
    C_IF,
    C_FUNCTION,
    C_RETURN,
    C_CALL
}CommandType;


typedef struct Parser Parser;

typedef struct Statement{
    char* command;
    size_t line_number;
    CommandType c_type;
    char* operation;
    char* operand_1;
    int operand_2;
} Statement;

Parser* init_parser(FILE* file);
Statement* next_statement(Parser* parser);
const char* current_command(Parser* parser);
void free_statement(Statement* stm);
void free_parser(Parser *parser);

#endif