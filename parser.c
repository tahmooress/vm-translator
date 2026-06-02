#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include <errno.h>
#include "reader.h"
#include "table.h"


#define MIN_INT  -32768 
#define MAX_INT 32767


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

typedef struct Parser{
    TokenSymbolTable* ctype_table;
    TokenSymbolTable* segments_table;
    Reader* reader;
    char* command;
} Parser;


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
char* current_command(Parser* parser);
void free_statement(Statement* expression);
void free_parser(Parser *parser);
static char* next_token(char** str);
static TokenSymbolTable* init_segments_symbol_table(void);
static TokenSymbolTable* init_ctype_symbol_table(void);
static int parse_int(const char *s, int *out);
static int parse_push_pop_statement(TokenSymbolTable* segments_table , Statement* stm, char* cmd);
static int parse_function_declaration_statement(TokenSymbolTable* segments_table , Statement* stm, char* cmd);
static int parse_function_call_statement(Statement* stm, char* cmd);
static int parse_lable_statement(Statement* stm, char* cmd);
static int parse_goto_statement(Statement* stm, char* cmd);
static int parse_if_statment(Statement* stm, char* cmd);

Parser* init_parser(FILE* file)
{   
    Reader* reader = reader_init(file);
    if(!reader) {
        fprintf(stderr, "init_reader failed");
        
        return NULL;
    }

    Parser *parser = (Parser*) malloc(sizeof(Parser));
    if (!parser){
        fprintf(stderr, "malloc parser failed");
        
        return NULL;
    }

    parser->reader = reader;

    parser->ctype_table = init_ctype_symbol_table();
    if (!parser->ctype_table) {
        fprintf(stderr, "init_ctype table failed");
        
        return NULL;
    }


    parser->segments_table = init_segments_symbol_table();
    if (!parser->segments_table) {
        fprintf(stderr, "init segments_table failed");
        token_symbol_table_free(parser->ctype_table);

        return NULL;
    }


    return parser;
}

char* current_command(Parser* parser)
{
    return parser->command;
}

Statement* next_statement(Parser* parser)
{   
    if (reader_advance(parser->reader) == EOF) 
    {
        return NULL;
    }

    parser->command = reader_get_line(parser->reader);
    char* cmd = parser->command;

    char* token;
    size_t line_number = reader_current_line_number(parser->reader);

    Statement* stm = (Statement*) malloc(sizeof(Statement));
    if (!stm) {
        return NULL;
    }

    stm->line_number = line_number;
    stm->command = parser->command;

    if ((token = next_token(&cmd)) != NULL)
    {   
        void* ctype;

       token_symbol_table_get(parser->ctype_table, token, &ctype);

        stm->c_type = *(CommandType*)ctype;
        stm->operation = token;
    }

    int code = 0;

    switch (stm->c_type)
    {
    case C_POP:
    case C_PUSH:
        code = parse_push_pop_statement(parser->segments_table, stm, cmd);
        break;
    case C_IF:
        code = parse_if_statment(stm, cmd);
        break;
    case C_GOTO:
        code = parse_goto_statement(stm, cmd);
        break;
    case C_LABEL:
        code = parse_lable_statement(stm, cmd);
        break;
    case C_FUNCTION:
        code = parse_function_declaration_statement(parser->segments_table, stm, cmd);
        break;
    case C_CALL:
        code = parse_function_call_statement(stm, cmd);
        break;
    case C_RETURN:
    case C_ARITHMETIC:
        break;
    default:
        fprintf(stderr, "line:%zu invalid token: %s\n", line_number, token);

        code = 5;
    }


    if (code) {
        free(token);
        free(stm);

        return NULL;
    }

    return stm;
}

void free_parser(Parser *parser)
{   
    if (!parser) return;

    if (parser->ctype_table) {
        token_symbol_table_free(parser->ctype_table);
    }

    if (parser->segments_table) {
        token_symbol_table_free(parser->segments_table);
    }
    
    if (parser->reader) {
        free(parser->reader);
    }
}

void free_statement(Statement* statement)
{
    if (!statement) {
        return;
    }

    if (statement->operation) {
        free(statement->operation);
    }

    free(statement);
}

static char* next_token(char** str)
{  
    if (!str || !*str){
        return NULL;
    }

    // trim prefix blanks
    while(**str && isspace((char)**str)) (*str)++;
    
    // iterate to reach to the first blank or end ofstring.
    char* next = *str;

    while(*next && !isspace((char)*next)) next++;
    size_t size = next - *str + 1;
    if (size == 1) {
        *str = next;
        return NULL;
    }

    char* cpy = (char*) malloc(sizeof(char) * size); 
    if (!cpy) return NULL;
    
    memcpy(cpy, *str, size);

    if (*next != '\0') {
        cpy[size -1] = '\0';
    }

    *str = next;

    return cpy;
}

static TokenSymbolTable* init_segments_symbol_table(void)
{
    TokenSymbolTable* table = token_symbol_table_init();
    if (!table) {
        return NULL;
    }

    char* token;
    char* segment;

    token = "local";
    segment = "LCL";
    token_symbol_table_set(table, token, (void*)(segment), sizeof(segment));
   
    token = "argument";
    segment = "ARG";
    token_symbol_table_set(table, token, (void*)(segment), sizeof(segment));

    token = "this";
    segment = "THIS";
    token_symbol_table_set(table, token, (void*)(segment), sizeof(segment));

    token = "that";
    segment = "THAT";
    token_symbol_table_set(table, token, (void*)(segment), sizeof(segment));

    token = "pointer";
    segment = "POINTER";
    token_symbol_table_set(table, token, (void*)(segment), sizeof(segment));

    token = "temp";
    segment = "TEMP";
    token_symbol_table_set(table, token, (void*)(segment), sizeof(segment));

    token = "static";
    segment = "static";
    token_symbol_table_set(table, token, (void*)(segment), sizeof(segment));
   
    token = "constant";
    segment = "constant";
    token_symbol_table_set(table, token, (void*)(segment), sizeof(segment));

    return table;
}

static TokenSymbolTable* init_ctype_symbol_table(void)
{   
    TokenSymbolTable* table = token_symbol_table_init();
    if (!table) {
        return NULL;
    }

    char* token;
    CommandType ctype;

    token = "push";
    ctype = C_PUSH;
    token_symbol_table_set(table, token, (void*)(&ctype), sizeof(ctype));

    token = "pop";
    ctype = C_POP;
    token_symbol_table_set(table, token, (void*)(&ctype), sizeof(ctype));

    token = "add";
    ctype = C_ARITHMETIC;
    token_symbol_table_set(table, token, (void*)(&ctype), sizeof(ctype));

    token = "sub";
    ctype = C_ARITHMETIC;
    token_symbol_table_set(table, token, (void*)(&ctype), sizeof(ctype));

    token = "neg";
    ctype = C_ARITHMETIC;
    token_symbol_table_set(table, token, (void*)(&ctype), sizeof(ctype));

    token = "eq";
    ctype = C_ARITHMETIC;
    token_symbol_table_set(table, token, (void*)(&ctype), sizeof(ctype));

    token = "gt";
    ctype = C_ARITHMETIC;
    token_symbol_table_set(table, token, (void*)(&ctype), sizeof(ctype));

    token = "lt";
    ctype = C_ARITHMETIC;
    token_symbol_table_set(table, token, (void*)(&ctype), sizeof(ctype));

    token = "and";
    ctype = C_ARITHMETIC;
    token_symbol_table_set(table, token, (void*)(&ctype), sizeof(ctype));
    
    token = "or";
    ctype = C_ARITHMETIC;
    token_symbol_table_set(table, token, (void*)(&ctype), sizeof(ctype));

    token = "not";
    ctype = C_ARITHMETIC;
    token_symbol_table_set(table, token, (void*)(&ctype), sizeof(ctype));

    token = "goto";
    ctype = C_GOTO;
    token_symbol_table_set(table, token, (void*)(&ctype), sizeof(ctype));

    token = "if-goto";
    ctype = C_IF;
    token_symbol_table_set(table, token, (void*)(&ctype), sizeof(ctype));

    token = "label";
    ctype = C_LABEL;
    token_symbol_table_set(table, token, (void*)(&ctype), sizeof(ctype));

    token = "function";
    ctype = C_FUNCTION;
    token_symbol_table_set(table, token, (void*)(&ctype), sizeof(ctype));    
    
    token = "call";
    ctype = C_CALL;
    token_symbol_table_set(table, token, (void*)(&ctype), sizeof(ctype));
    
    token = "return";
    ctype = C_RETURN;
    token_symbol_table_set(table, token, (void*)(&ctype), sizeof(ctype));    
    
    return table;
}

static int parse_int(const char *s, int *out) 
{
    char *end = NULL;
    long val;

    if (s == NULL || *s == '\0') return 0;

    errno = 0;
    val = strtol(s, &end, 10);                
    if (errno == ERANGE || val < MIN_INT || val > MAX_INT) return 1;
    if (end == s) return 1;                  


    while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r' || *end == '\f' || *end == '\v')
        end++;
    if (*end != '\0') return 1;              

    *out = (int)val;
    return 0;
}

static int parse_push_pop_statement(TokenSymbolTable* segments_table , Statement* stm, char* cmd)
{   
    char* token;
    token = next_token(&cmd);

    if (token == NULL)
    {
        fprintf(stderr, "line:%zu expect segment but is empty\n", stm->line_number);

        return 1;
    }

    void* segment;

    if (!token_symbol_table_get(segments_table, token, &segment)) 
    {
        fprintf(stderr, "line:%zu invalid segment: %s\n", stm->line_number, token);
        free(token);

        return 1;
    }

    stm->operand_1 = (char*)segment;

    token = next_token(&cmd);
    if (token == NULL)
    {
        fprintf(stderr, "line:%zu expect segmentValue but is empty\n", stm->line_number);

        return 1;
    }

    if (parse_int(token, &stm->operand_2)) 
    {
        fprintf(stderr, "line:%zu expect numeric token:%s", stm->line_number ,token);
        free(token);

        return 1;
    }

    if (stm->operand_1 && strcmp(stm->operand_1, "constant") != 0) 
    {
        if (stm->operand_2 < 0) 
        {
            fprintf(stderr, "line:%zu segment index should be a posivitive number, segment:%s value:%d", stm->line_number, stm->operand_1, stm->operand_2);
            free(token);

            return 1;
        }
    }

    return 0;
}

static int parse_function_declaration_statement(TokenSymbolTable* segments_table , Statement* stm, char* cmd)
{
    char* token;
    token = next_token(&cmd);

    if (token == NULL)
    {
        fprintf(stderr, "line:%zu expect functionName but is empty\n", stm->line_number);

        return 1;
    }

    stm->operand_1 = (char*)token;

    token = next_token(&cmd);
    if (token == NULL)
    {
        fprintf(stderr, "line:%zu expect functionArgs but is empty\n", stm->line_number);
        
        return 1;
    }
    
    if (parse_int(token, &stm->operand_2)) 
    {
        fprintf(stderr, "line:%zu function argsNumber should be numeric, token:%s\n", stm->line_number ,token);
        free(token);

        return 1;
    }

    return 0;
}

static int parse_function_call_statement(Statement* stm, char* cmd)
{
    char* token;
    token = next_token(&cmd);

    if (token == NULL)
    {
        fprintf(stderr, "line:%zu expect functionName but is empty\n", stm->line_number);

        return 1;
    }

    stm->operand_1 = (char*)token;

    token = next_token(&cmd);
    if (token == NULL)
    {
        fprintf(stderr, "line:%zu expect functionArgs but is empty\n", stm->line_number);

        return 1;
    }
    
    if (parse_int(token, &stm->operand_2)) 
    {
        fprintf(stderr, "line:%zu function argsNumber should be numeric, token:%s\n", stm->line_number ,token);
        free(token);

        return 1;
    }

    return 0;
}

static int parse_lable_statement(Statement* stm, char* cmd)
{
    char* token;
    token = next_token(&cmd);

    if (token == NULL)
    {
        fprintf(stderr, "line:%zu expect labelName but is empty\n", stm->line_number);

        return 1;
    }

    stm->operand_1 = (char*)token;

    return 0;
}

static int parse_goto_statement(Statement* stm, char* cmd)
{
    char* token;
    token = next_token(&cmd);

    if (token == NULL)
    {
        fprintf(stderr, "line:%zu expect labelName but is empty\n", stm->line_number);

        return 1;
    }

    stm->operand_1 = (char*)token;

    return 0;
}

static int parse_if_statment(Statement* stm, char* cmd)
{
    char* token;
    token = next_token(&cmd);

    if (token == NULL)
    {
        fprintf(stderr, "line:%zu expect labelName but is empty\n", stm->line_number);

        return 1;
    }

    stm->operand_1 = (char*)token;

    return 0;
}