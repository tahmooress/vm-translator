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
    C_LABLE,
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
    if (reader_advance(parser->reader) == EOF) {
        return NULL;
    }

    parser->command = reader_get_line(parser->reader);
    char* cmd = parser->command;

    char* token;
    int line_number = reader_current_line_number(parser->reader);

    Statement* expr = (Statement*) malloc(sizeof(Statement));
    if (!expr) {
        return NULL;
    }

    expr->command = cmd;

    if ((token = next_token(&cmd)) != NULL)
    {   
        void* ctype;

        if (!token_symbol_table_get(parser->ctype_table, token, &ctype)) {
            fprintf(stderr, "line:%d invalid operation type:%s", line_number,token);
            free(token);
            free(expr);

            return NULL;
        }

        expr->c_type = *(CommandType*)ctype;
        expr->operation = token;
    }

    if ((token = next_token(&cmd)) != NULL)
    {   
        void* opr1;

        if (!token_symbol_table_get(parser->segments_table, token, &opr1)) {
            fprintf(stderr, "line:%d invalid segment: %s", line_number, token);
            free(token);
            free(expr);

            return NULL;
        }

        expr->operand_1 = (char*)opr1;
        free(token);
    }    

    if ((token = next_token(&cmd)) != NULL)
    {   
        int val;

        if (parse_int(token, &val)) {
            fprintf(stderr, "line:%d expect numeric token:%s", line_number ,token);
            free(token);
            free(expr);

            return NULL;
        }

        if (expr->operand_1 && strcmp(expr->operand_1, "constant") != 0) {
            if (val < 0) {
                fprintf(stderr, "line:%d segment index should be a posivitive number, segment:%s index:%d", line_number, expr->operand_1, val);
                free(token);
                free(expr);

                return NULL;
            }
        }

        expr->operand_2 = val;
    } 

    return expr;
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

    token = "lable";
    ctype = C_LABLE;
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

static int parse_int(const char *s, int *out) {
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