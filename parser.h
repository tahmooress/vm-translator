#ifndef PARSER_H
#define PARSER_H
#include<stdio.h>
#include"./table.h"

#define C_LCL "local"
#define C_ARG "argument"
#define C_THIS "this"
#define C_THAT "that"
#define C_POINTER "pointer"
#define C_TMP "temp"
#define C_STATIC "static"
#define C_CONSTANT "constant"

typedef enum {
    C_ARITHMETIC,
    C_PUSH,
    C_POP,
    C_LABLE,
    C_GOTO,
    C_IF,
    C_FUNCTION,
    C_RETURN,
    C_CALL
}CommandType;


typedef struct Parser Parser;

typedef struct Statement{
    char* command;
    CommandType c_type;
    char* operation;
    char* operand_1;
    int operand_2;
} Statement;

Parser* init_parser(FILE* file);
Statement* next_expression(Parser* parser);
const char* current_command(Parser* parser);
void free_expresion(Statement* expression);
void free_parser(Parser *parser);

#endif