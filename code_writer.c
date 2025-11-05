#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include"./parser.h"
#include"./table.h"

typedef enum {
    VM_ANNOTATE = 1u << 0,
    VM_DEBUG    = 1u << 1,
} Flags;


typedef struct CodeWriter{
    FILE* out;
    const char* filename;
    int anotate;
    int debug;
    int counter;
}CodeWriter;

static int write_push_pop(CodeWriter* code_writer ,Statement* expr);
static int write_vm_arithmetic(CodeWriter* code_writer ,Statement* expr);

static size_t write_vm_pop(FILE* out, const char* segment, const char* offset);
static size_t write_vm_pop_static(FILE* out, const char* offset);
static size_t wirte_vm_push_constant(FILE* out, const char* value);
static size_t write_vm_push(FILE* out, const char* segment, const char* offset);
static size_t write_vm_push_static(FILE* out, const char* offset);
static size_t write_logical_not(FILE* out);
static size_t write_logical_or(FILE* out, int counter);
static size_t write_logical_and(FILE* out, int counter);
static size_t write_logical_eq(FILE* out, int counter);
static size_t write_logical_lt(FILE* out, int counter);
static size_t write_logical_gt(FILE* out, int counter);
static size_t write_arithmetic_negate(FILE* out);
static size_t write_arithmetic_sub(FILE* out);
static size_t write_arithmetic_add(FILE* out);
static size_t vm_segment_initializer(FILE* out);


CodeWriter* init_code_writer(FILE * out, const char* filename,Flags flags);
size_t write_halt(FILE* out);
int write_code(CodeWriter* code_writer ,Statement* stm);

CodeWriter* init_code_writer(FILE * out, const char* filename,Flags flags)
{
    CodeWriter* c = (CodeWriter*) malloc(sizeof(CodeWriter));
    if (!c) return NULL;

    c->out = out;
    c->filename = filename;
    c->anotate = flags&VM_ANNOTATE;
    c->debug = flags&VM_DEBUG;

    vm_segment_initializer(out);

    return c;
}

int write_code(CodeWriter* code_writer ,Statement* stm)
{   
    if (code_writer->anotate) {
        fprintf(code_writer->out, "// %s\n", stm->command);
    }

    switch (stm->c_type)
    {
    case C_PUSH:
    case C_POP:
      return write_push_pop(code_writer, stm);
    case C_ARITHMETIC:
        return write_vm_arithmetic(code_writer, stm); 
    default:
        return 1;
    }

    return 1;
}

size_t write_halt(FILE* out)
{
    size_t n = 0;

    n += fprintf(out,
        "(END)\n"
        "@END\n"
        "0;JMP\n"
    );

    return n;
}

static int write_push_pop(CodeWriter* code_writer ,Statement* expr)
{
    char buf[32];                            
    
    if (strcmp(expr->operand_1, C_STATIC) == 0) {
        snprintf(buf, sizeof buf, "%s.%d", code_writer->filename,expr->operand_2);
    }else {
        snprintf(buf, sizeof buf, "%d", expr->operand_2);
    }

    
    if (expr->c_type == C_PUSH)
    {
        if (strcmp(expr->operand_1, C_CONSTANT) == 0)
        {
            wirte_vm_push_constant(code_writer->out, buf);

            return 0;
        }else if (strcmp(expr->operand_1, C_STATIC) == 0) {
            write_vm_push_static(code_writer->out, buf);

            return 0;
        }

        write_vm_push(code_writer->out, expr->operand_1, buf);

        return 0;
    }

    if (strcmp(expr->operand_1, C_STATIC) == 0)
    {
        write_vm_pop_static(code_writer->out, buf);

        return 0;
    }

    write_vm_pop(code_writer->out, expr->operand_1, buf);

    return 0;    
}

static int write_vm_arithmetic(CodeWriter* code_writer ,Statement* expr)
{   
    char* operation = expr->operation;
    FILE* out = code_writer->out;

    if (strcmp(operation, "add") == 0) {
        return write_arithmetic_add(out);
    }

    if (strcmp(operation, "sub") == 0) {
        return write_arithmetic_sub(out);
    }

    if (strcmp(operation, "neg") == 0) {
        return write_arithmetic_negate(out);
    }

    if (strcmp(operation, "gt") == 0) {
        code_writer->counter++;
        return write_logical_gt(out, code_writer->counter);
    }    

    if (strcmp(operation, "lt") == 0) {
        code_writer->counter++;
        return write_logical_lt(out, code_writer->counter);
    }

    if (strcmp(operation, "eq") == 0) {
        code_writer->counter++;
        return write_logical_eq(out, code_writer->counter);
    }    

    if (strcmp(operation, "and") == 0) {
        code_writer->counter++;
        return write_logical_or(out, code_writer->counter);
    }
    
    if (strcmp(operation, "or") == 0) {
        code_writer->counter++;
        return write_logical_and(out, code_writer->counter);
    } 
    
    if (strcmp(operation, "not") == 0) {
        return write_logical_not(out);
    }
    
    return 0;
}


static size_t write_vm_push(FILE* out, const char* segment, const char* offset)
{
    size_t n = 0;

    n += fprintf(out,
        "@%s\n"             
        "D=A\n"                     
        "@%s\n"           
        "A=D+M\n"                   
        "D=M\n"                     
        "@SP\n"                     
        "A=M\n"                     
        "M=D\n"                     
        "@SP\n"                     
        "M=M+1\n", offset,segment);
    
    return n;
}

static size_t write_vm_push_static(FILE* out, const char* offset)
{
    size_t n = 0;

    n += fprintf(out,
        "@%s\n"             
        "D=M\n"                                       
        "@SP\n"                     
        "A=M\n"                     
        "M=D\n"                     
        "@SP\n"                     
        "M=M+1\n", offset);
    
    return n;
}

static size_t wirte_vm_push_constant(FILE* out, const char* value)
{
    size_t n = 0;
    
    n += fprintf(out,
        "@%s\n"             
        "D=A\n"                     
        "@SP\n"                     
        "A=M\n"                     
        "M=D\n"                     
        "@SP\n"                     
        "M=M+1\n", value);
    
    return n;
}

static size_t write_vm_pop(FILE* out, const char* segment, const char* offset)
{
    size_t n = 0;

    n += fprintf(out,
        "@%s\n"             
        "D=A\n"                     
        "@%s\n"           
        "D=D+M\n"                   
        "@R5\n"                     
        "M=D\n"                     
        "@SP\n"                     
        "AM=M-1\n"                  
        "D=M\n"                     
        "@R5\n"                     
        "A=M\n"                     
        "M=D\n", offset, segment); 
    
    return n;
}

static size_t write_vm_pop_static(FILE* out, const char* offset)
{
    size_t n = 0;

    n += fprintf(out,
        "@SP\n"             
        "AM=M-1\n"                     
        "D=M\n"
        "@%s\n"
        "M=D\n", offset); 
    
    return n;
}

static size_t write_arithmetic_add(FILE* out)
{   
    size_t n = 0;

    n += fprintf(out,
        "@SP\n"             
        "AM=M-1\n"                     
        "D=M\n"
        "A=A-1\n"
        "M=D+M\n"
    ); 
    
    return n;
}

static size_t write_arithmetic_sub(FILE* out)
{   
    size_t n = 0;

    n += fprintf(out,
        "@SP\n"             
        "AM=M-1\n"                     
        "D=M\n"
        "A=A-1\n"
        "M=M-D\n"
    ); 
    
    return n;
}

static size_t write_arithmetic_negate(FILE* out)
{
    size_t n = 0;

    n += fprintf(out,
        "@SP\n"
        "AM=M-1\n"
        "M=-M\n"
    );

    return n;
}

static size_t write_logical_gt(FILE* out, int counter)
{
    size_t n = 0;

    n += fprintf(out,
        "@SP\n"
        "AM=M-1\n"
        "D=M\n"
        "A=A-1\n"
        "D=D-M\n"
        "@GT_TRUE.%d\n"
        "D;JGT\n"
        "@SP\n"
        "A=M-1\n"
        "M=0\n"
        "@GT_END.%d\n"
        "0;JMP\n"
        "(GT_TRUE.%d)\n"
        "@SP\n"
        "A=M-1\n"
        "M=-1\n"    
        "(GT_END.%d)\n", counter,counter,counter,counter);

    return n;
}

static size_t write_logical_lt(FILE* out, int counter)
{
    size_t n = 0;

    n += fprintf(out,
        "@SP\n"
        "AM=M-1\n"
        "D=M\n"
        "A=A-1\n"
        "D=D-M\n"
        "@LT_TRUE.%d\n"
        "D;JLT\n"
        "@SP\n"
        "A=M-1\n"
        "M=0\n"
        "@END_LT.%d\n"
        "0;JMP\n"
        "(LT_TRUE.%d)\n"
        "M=-1\n"    
        "(END_LT.%d)\n", counter, counter, counter, counter);

    return n;
}

static size_t write_logical_eq(FILE* out, int counter)
{
    size_t n = 0;

    n += fprintf(out,
        "@SP\n"
        "AM=M-1\n"
        "D=M\n"
        "A=A-1\n"
        "D=D-M\n"
        "@EQ_TRUE.%d\n"
        "D;JEQ\n"
        "@SP\n"
        "A=M-1\n"
        "M=0\n"
        "@END_EQ.%d\n"
        "0;JMP\n"
        "(EQ_TRUE.%d)\n"
        "M=-1\n"    
        "(END_EQ.%d)\n", counter, counter, counter, counter);

    return n;
}

static size_t write_logical_and(FILE* out, int counter)
{
    size_t n = 0;

    n += fprintf(out,
        "@SP\n"
        "AM=M-1\n"
        "D=M\n"
        "A=A-1\n"
        "D=D&M\n"
        "@AND_TRUE.%d\n"
        "D;JEQ\n"
        "M=0\n" 
        "@END_AND.%d\n"
        "0;JMP\n"
        "(AND_TRUE.%d)\n"
        "M=-1\n"
        "(END_AND.%d)\n", counter, counter, counter, counter);

    return n;
}

static size_t write_logical_or(FILE* out, int counter)
{
    size_t n = 0;

    n += fprintf(out,
        "@SP\n"
        "AM=M-1\n"
        "D=M\n"
        "A=A-1\n"
        "D=D|M\n"
        "@OR_TURE.%d\n"
        "D;JEQ\n"
        "M=0\n"
        "@END_OR.%d\n"
        "0;JMP\n"
        "(OR_TURE.%d)\n"
        "M=-1\n"    
        "(END_OR.%d)\n",  counter, counter, counter, counter);

    return n;
}

static size_t write_logical_not(FILE* out)
{
    size_t n = 0;

    n += fprintf(out,
        "@SP\n"
        "AM=M-1\n"
        "M=!M\n"
    );


    return n;
}

static size_t vm_segment_initializer(FILE* out)
{
/*
set sp 256,        // stack pointer
set local 300,     // base address of the local segment
set argument 400,  // base address of the argument segment
set this 3000,     // base address of the this segment
set that 3010, 
*/

    size_t n = 0;

    // set sp 256
    n += fprintf(out,
        "@256\n"
        "D=A\n"
        "@SP\n"
        "M=D\n"
    );

    // set local 300
    n += fprintf(out,
        "@300\n"
        "D=A\n"
        "@LCL\n"
        "M=D\n"
    );

    // set argument 400
    n += fprintf(out,
        "@400\n"
        "D=A\n"
        "@ARG\n"
        "M=D\n"
    );

    // set this 3000
    n += fprintf(out,
        "@3000\n"
        "D=A\n"
        "@THIS\n"
        "M=D\n"
    );    

    // set that 3010
    n += fprintf(out,
        "@3010\n"
        "D=A\n"
        "@THAT\n"
        "M=D\n"
    );  

    return n;
}