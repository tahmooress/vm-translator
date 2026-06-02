#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include <stdarg.h>
#include"./parser.h"
#include"./table.h"
#include"./code_writer.h"


const int UNEXPECTED_ERR = -1; 

const int TEMP_RAM_OFFSET = 5;

typedef struct Scope Scope;

typedef struct Scope{
    const char* func_name;
    size_t counter;
    Scope* previous;
    Scope* next;
}Scope;

typedef struct CodeWriter{
    Scope* current_scope;
    FILE* out;
    const char* filename;
    int anotate;
    int debug;
    size_t counter;
    size_t instruction_counter;
}CodeWriter;

// use for [label, __goto, if_goto].
const char normal_lable_temlate[] = "%s$%s";
const int normal_lable_temlate_len = 1;
// use for return address of function call.
const char return_addr_label_template[] = "%s$ret.%zu";
const int return_addr_label_template_len = 5;
// use for label for start of function declaration.
const char function_declaration_label_template[] = "%s";
const int function_declaration_label_template_len = 0;

static int temp_mmap(int offset);
static char* templated_label_creator(size_t dest_size, const char* fmt, ...);
size_t n_digit(size_t num);
static Scope* remove_scope_from_tail(Scope* tail);
static Scope* add_scope_to_tail(Scope* tail, const char* func_name);

static int function_return(FILE* out);
static int function_call(FILE* out, char* func_name, int nArgs, Scope* scope);
static int function_declaration(FILE* out, char* func_name, int nVars);
static int if_goto(FILE* out, char* label_name);
static int __goto(FILE* out, char* label_name);
static int label(FILE* out, char* label_name);
static int logical_not(FILE* out);
static int logical_or(FILE* out, Scope* scope);
static int logical_and(FILE* out, Scope* scope);
static int logical_eq(FILE* out, Scope* scope);
static int logical_lt(FILE* out, Scope* scope);
static int logical_gt(FILE* out, Scope* scope);
static int arithmetic_negate(FILE* out);
static int arithmetic_sub(FILE* out);
static int arithmetic_add(FILE* out);
static int pop_temp(FILE* out, int offset);
static int pop_static(FILE* out, const char* filename, int offset);
static int pop_pointer(FILE* out, int offset);
static int pop(FILE* out, const char* segment, int offset);
static int push_constant(FILE* out, int value);
static int push_temp(FILE* ou, int offset);
static int push_static(FILE* out, const char* filename ,int offset);
static int push_pointer(FILE* out, int offset);
static int push(FILE* out, const char* segment, int offset);
static int arithmetic(CodeWriter* code_writer ,Statement* expr);
static int push_pop(CodeWriter* code_writer , Statement* stm);

CodeWriter* init_code_writer(FILE * out, const char* filename,Flags flags)
{
    CodeWriter* c = (CodeWriter*) malloc(sizeof(CodeWriter));
    if (!c) return NULL;

    c->out = out;
    c->filename = filename;
    c->anotate = flags&VM_ANNOTATE;
    c->debug = flags&VM_DEBUG;
    c->counter = 1;
    c->instruction_counter = 1;
    c->current_scope = add_scope_to_tail(NULL, "GLOBAL");

    return c;
}

void free_code_writer(CodeWriter* code_writer)
{
    free(code_writer->current_scope);
}

int write_code(CodeWriter* code_writer, Statement* stm)
{
    if (code_writer->anotate) {
        fprintf(code_writer->out, "// %s\n", stm->command);
    }

    switch (stm->c_type)
    {
    case C_PUSH:
    case C_POP:
        {   
            int err = push_pop(code_writer, stm);
            if (err == UNEXPECTED_ERR)
            {
                fprintf(stderr, "failed to translate command: %s on line:%zu\n", stm->command, stm->line_number);
                return err;
            }
       
            return 0;
        }
    case C_ARITHMETIC:
        {   
            int err = arithmetic(code_writer, stm);
            if (err == UNEXPECTED_ERR) 
            {
                fprintf(stderr, "failed to translate command: %s on line:%zu\n", stm->command, stm->line_number);
                return err;
            }
        
            return 0;
        }
    case C_FUNCTION:
        {
            Scope* new_scope = add_scope_to_tail(code_writer->current_scope, stm->operand_1);
            if (!new_scope) return UNEXPECTED_ERR;

            code_writer->current_scope = new_scope;

            int err = function_declaration(code_writer->out, stm->operand_1, stm->operand_2);
            if (err == UNEXPECTED_ERR) {
                fprintf(stderr, "failed to translate command: %s on line:%zu\n", stm->command, stm->line_number);
                return err;
            }

            return 0;
        }
    case C_CALL:
        {
            int err = function_call(code_writer->out, stm->operand_1, stm->operand_2, code_writer->current_scope);
            if (err == UNEXPECTED_ERR) {
                fprintf(stderr, "failed to translate command: %s on line:%zu\n", stm->command, stm->line_number);
                return err;
            }
            
            // change scope to the new calling function.
            // Scope* new_scope = add_scope_to_tail(code_writer->current_scope, stm->operand_1);
            // if (!new_scope) return UNEXPECTED_ERR;

            // code_writer->current_scope = new_scope;

            return 0;
        }
    case C_RETURN:
        {
            int err = function_return(code_writer->out);
            if (err == UNEXPECTED_ERR) {
                fprintf(stderr, "failed to translate command: %s on line:%zu\n", stm->command, stm->line_number);
                return err;
            }

            Scope* new_scope = remove_scope_from_tail(code_writer->current_scope);
            if (!new_scope) return UNEXPECTED_ERR;

            code_writer->current_scope = new_scope;
        
            return 0;
        }
    case C_LABEL:
        {
            size_t dest_size = strlen(code_writer->current_scope->func_name) + normal_lable_temlate_len + strlen(stm->operand_1) + 1;
            char* label_name = templated_label_creator(dest_size, normal_lable_temlate, code_writer->current_scope->func_name, stm->operand_1);
            if (!label_name) return UNEXPECTED_ERR;
            
            int err = label(code_writer->out, label_name);
            if (err == UNEXPECTED_ERR) {
                fprintf(stderr, "failed to translate command: %s on line:%zu\n", stm->command, stm->line_number);
                return err;
            }

            free(label_name);
            
            return 0;  
        }
    case C_GOTO:
        {
            size_t dest_size = strlen(code_writer->current_scope->func_name) + normal_lable_temlate_len + strlen(stm->operand_1) + 1;
            char* label_name = templated_label_creator(dest_size, normal_lable_temlate, code_writer->current_scope->func_name, stm->operand_1);
            if (!label_name) return UNEXPECTED_ERR;
            
            __goto(code_writer->out, label_name);

            free(label_name);
            
            return 0;
        }
    case C_IF:
        {
            size_t dest_size = strlen(code_writer->current_scope->func_name) + normal_lable_temlate_len + strlen(stm->operand_1) + 1;
            char* label_name = templated_label_creator(dest_size, normal_lable_temlate, code_writer->current_scope->func_name, stm->operand_1);
            if (!label_name) return UNEXPECTED_ERR;
            
            int err = if_goto(code_writer->out, label_name);
            if (err == UNEXPECTED_ERR ) {
                fprintf(stderr, "failed to translate command: %s on line:%zu\n", stm->command, stm->line_number);

                return err;
            }

            free(label_name);
            
            return 0;
        }
    }

    return UNEXPECTED_ERR;
}

void write_halt(FILE* out)
{
    fprintf(out,
        "(END)\n"
        "@END\n"
        "0;JMP\n"
    );
}

void segment_initializer(CodeWriter* code_writer)
{
/*
set sp 256,        // stack pointer
set local 300,     // base address of the local segment
set argument 400,  // base address of the argument segment
set this 3000,     // base address of the this segment
set that 3010, 
*/

    // set sp 256
    fprintf(code_writer->out,
        "@256\n"
        "D=A\n"
        "@SP\n"
        "M=D\n"
    );

    // set local 300
    fprintf(code_writer->out,
        "@300\n"
        "D=A\n"
        "@LCL\n"
        "M=D\n"
    );

    // set argument 400
    fprintf(code_writer->out,
        "@400\n"
        "D=A\n"
        "@ARG\n"
        "M=D\n"
    );

    // set this 3000
    fprintf(code_writer->out,
        "@3000\n"
        "D=A\n"
        "@THIS\n"
        "M=D\n"
    );    

    // set that 3010
    fprintf(code_writer->out,
        "@3010\n"
        "D=A\n"
        "@THAT\n"
        "M=D\n"
    );  

    code_writer->instruction_counter = 20;
}

static int push_pop(CodeWriter* code_writer , Statement* stm)
{
    if (stm->c_type == C_PUSH)
    {
        if (strcmp(stm->operand_1, C_STATIC) == 0)
            return push_static(code_writer->out, code_writer->filename, stm->operand_2);
        
        if (strcmp(stm->operand_1, C_CONSTANT) == 0)
            return push_constant(code_writer->out, stm->operand_2);

        if (strcmp(stm->operand_1, C_POINTER) == 0)
            return push_pointer(code_writer->out, stm->operand_2);

        if ((strcmp(stm->operand_1, C_TMP) == 0))
        {   
            int offset = temp_mmap(stm->operand_2);
            if (offset == UNEXPECTED_ERR)
            {
                fprintf(stderr, "line: %zu\tcode: %s\n\tsegment violation: TEMP offset should between 0-7\n",
                    stm->line_number, stm->command);
                return UNEXPECTED_ERR;
            }

            return push_temp(code_writer->out, offset);
        }

        return push(code_writer->out, stm->operand_1, stm->operand_2);
    }

    if (stm->c_type == C_POP)
    {
        if (strcmp(stm->operand_1, C_STATIC) == 0) 
            return pop_static(code_writer->out, code_writer->filename, stm->operand_2);
        
        if (strcmp(stm->operand_1, C_POINTER) == 0)
            return pop_pointer(code_writer->out, stm->operand_2);

        if ((strcmp(stm->operand_1, C_TMP) == 0))
        {   
            int offset = temp_mmap(stm->operand_2);
            if (offset == UNEXPECTED_ERR)
            {
                fprintf(stderr, "line: %zu\tcode: %s\n\tsegment violation: TEMP offset should between 0-7\n",
                    stm->line_number, stm->command);
                return UNEXPECTED_ERR;
            }

            return pop_temp(code_writer->out, offset);
        }    

        return pop(code_writer->out, stm->operand_1, stm->operand_2);
    }

    fprintf(stderr, "invalid push_pop operation file_name: %s line_number: %zu, command: %s\n",
        code_writer->filename, stm->line_number ,stm->command);
   
    return UNEXPECTED_ERR;
}

static int arithmetic(CodeWriter* code_writer ,Statement* expr)
{   
    char* operation = expr->operation;
    FILE* out = code_writer->out;

    if (strcmp(operation, "add") == 0) {
        return arithmetic_add(out);
    }

    if (strcmp(operation, "sub") == 0) {
        return arithmetic_sub(out);
    }

    if (strcmp(operation, "neg") == 0) {
        return arithmetic_negate(out);
    }

    if (strcmp(operation, "gt") == 0) {
        int err;
        err = logical_gt(out, code_writer->current_scope);
        code_writer->current_scope++;
        return  err;
    }    

    if (strcmp(operation, "lt") == 0) {
        int err;
        err = logical_lt(out, code_writer->current_scope);
        code_writer->current_scope++;
        return  err;
    }

    if (strcmp(operation, "eq") == 0) {
        int err;
        err = logical_eq(out, code_writer->current_scope);
        code_writer->current_scope++;
        return  err;        
    }    

    if (strcmp(operation, "and") == 0) {
        int err;
        err = logical_or(out, code_writer->current_scope);
        code_writer->current_scope++;
        return  err;              
    }
    
    if (strcmp(operation, "or") == 0) {
        int err;
        err = logical_and(out, code_writer->current_scope);
        code_writer->current_scope++;
        return  err;          
    } 
    
    if (strcmp(operation, "not") == 0) {
        return logical_not(out);
    }

    return UNEXPECTED_ERR;
}

static int push(FILE* out, const char* segment, int offset)
{
    fprintf(out,
        "@%d\n"             
        "D=A\n"                     
        "@%s\n"           
        "A=D+M\n"                   
        "D=M\n"                     
        "@SP\n"                     
        "A=M\n"                     
        "M=D\n"                     
        "@SP\n"                     
        "M=M+1\n", offset, segment);

    return 0;
}

static int push_temp(FILE* out, int offset)
{
    fprintf(out,
        "@%d\n"
        "D=M\n"
        "@SP\n"
        "A=M\n"
        "M=D\n"
        "@SP\n"
        "M=M+1\n", offset
    );

    return 0;
}

static int push_static(FILE* out, const char* filename ,int offset)
{
    fprintf(out,
        "@%s.%d\n"             
        "D=M\n"                                       
        "@SP\n"                     
        "A=M\n"                     
        "M=D\n"                     
        "@SP\n"                     
        "M=M+1\n", filename, offset);
    
    return 0;
}

static int push_constant(FILE* out, int value)
{
    fprintf(out,
        "@%d\n"             
        "D=A\n"                     
        "@SP\n"                     
        "A=M\n"                     
        "M=D\n"                     
        "@SP\n"                     
        "M=M+1\n", value);

    return 0;
}

static int push_pointer(FILE* out, int offset)
{   
    char* this_pointer = "THIS";
    char* that_pointer = "THAT";

    char* dest;

    switch (offset)
    {
    case 0:
        dest = this_pointer;
        break;
    case 1:
        dest = that_pointer;
        break;
    default:
        return UNEXPECTED_ERR;
    }

    fprintf(out,
        "@%s\n"
        "D=M\n"
        "@SP\n"
        "A=M\n"
        "M=D\n"
        "@SP\n"
        "M=M+1\n", dest
    );

    return 0;
}

static int pop(FILE* out, const char* segment, int offset)
{
    fprintf(out,
        "@%d\n"             
        "D=A\n"                     
        "@%s\n"           
        "D=D+M\n"                   
        "@R15\n"               
        "M=D\n"                     
        "@SP\n"                     
        "AM=M-1\n"                  
        "D=M\n"                     
        "@R15\n"                     
        "A=M\n"                     
        "M=D\n", offset, segment);

    return 0;
}

static int pop_temp(FILE* out, int offset)
{
    fprintf(out,
        "@SP\n"
        "AM=M-1\n"
        "D=M\n"
        "@%d\n"
        "M=D\n", offset
    );

    return 0;
}

static int pop_static(FILE* out, const char* filename, int offset)
{
    fprintf(out,
        "@SP\n"             
        "AM=M-1\n"                     
        "D=M\n"
        "@%s.%d\n"
        "M=D\n", filename , offset);
    
    return 0;
}

static int pop_pointer(FILE* out, int offset)
{   
    char* this_pointer = "THIS";
    char* that_pointer = "THAT";

    char* dest;

    switch (offset)
    {
    case 0:
        dest = this_pointer;
        break;
    case 1:
        dest = that_pointer;
        break;
    default:
        return UNEXPECTED_ERR;
    }

    fprintf(out,
        "@SP\n"
        "AM=M-1\n"
        "D=M\n"
        "@%s\n"
        "M=D\n", dest
    );

    return 0;
}

static int arithmetic_add(FILE* out)
{   
    fprintf(out,
        "@SP\n"             
        "AM=M-1\n"                     
        "D=M\n"
        "A=A-1\n"
        "M=D+M\n"
    ); 

    return 0;
}

static int arithmetic_sub(FILE* out)
{   
    fprintf(out,
        "@SP\n"             
        "AM=M-1\n"                     
        "D=M\n"
        "A=A-1\n"
        "M=M-D\n"
    ); 

    return 0;
}

static int arithmetic_negate(FILE* out)
{
    fprintf(out,
        "@SP\n"
        "AM=M-1\n"
        "M=-M\n"
    );

    return 0;
}

static int logical_gt(FILE* out, Scope* scope)
{   
    fprintf(out,
        "@SP\n"
        "AM=M-1\n"
        "D=M\n"
        "A=A-1\n"
        "D=D-M\n"
        "@%s$GT_TRUE.%zu\n"
        "D;JGT\n"
        "@SP\n"
        "A=M-1\n"
        "M=0\n"
        "@%s$GT_END.%zu\n"
        "0;JMP\n"
        "(%s$GT_TRUE.%zu)\n"
        "@SP\n"
        "A=M-1\n"
        "M=-1\n"    
        "(%s$GT_END.%zu)\n", 
            scope->func_name, scope->counter, scope->func_name, scope->counter,
            scope->func_name, scope->counter, scope->func_name, scope->counter
    );     
    
    return 0;
}

static int logical_lt(FILE* out, Scope* scope)
{   
    fprintf(out,
        "@SP\n"
        "AM=M-1\n"
        "D=M\n"
        "A=A-1\n"
        "D=D-M\n"
        "@%s$LT_TRUE.%zu\n"
        "D;JLT\n"
        "@SP\n"
        "A=M-1\n"
        "M=0\n"
        "@%s$END_LT.%zu\n"
        "0;JMP\n"
        "(%s$LT_TRUE.%zu)\n"
        "M=-1\n"    
        "(%s$END_LT.%zu)\n",
            scope->func_name, scope->counter, scope->func_name, scope->counter,
            scope->func_name, scope->counter, scope->func_name, scope->counter
    );      
    
    return 0;
}

static int logical_eq(FILE* out, Scope* scope)
{
    fprintf(out,
        "@SP\n"
        "AM=M-1\n"
        "D=M\n"
        "A=A-1\n"
        "D=D-M\n"
        "@%s$EQ_TRUE.%zu\n"
        "D;JEQ\n"
        "@SP\n"
        "A=M-1\n"
        "M=0\n"
        "@%s$END_EQ.%zu\n"
        "0;JMP\n"
        "(%s$EQ_TRUE.%zu)\n"
        "M=-1\n"    
        "(%s$END_EQ.%zu)\n",
            scope->func_name, scope->counter, scope->func_name, scope->counter,
            scope->func_name, scope->counter, scope->func_name, scope->counter
    );  
    
    return 0;
}

static int logical_and(FILE* out, Scope* scope)
{
    fprintf(out,
        "@SP\n"
        "AM=M-1\n"
        "D=M\n"
        "A=A-1\n"
        "D=D&M\n"
        "@%s$AND_TRUE.%zu\n"
        "D;JEQ\n"
        "M=0\n" 
        "@%s$END_AND.%zu\n"
        "0;JMP\n"
        "(%s$AND_TRUE.%zu)\n"
        "M=-1\n"
        "(%s$END_AND.%zu)\n",
            scope->func_name, scope->counter, scope->func_name, scope->counter,
            scope->func_name, scope->counter, scope->func_name, scope->counter
    );
    
    return 0;
}

static int logical_or(FILE* out, Scope* scope)
{   
    fprintf(out,
        "@SP\n"
        "AM=M-1\n"
        "D=M\n"
        "A=A-1\n"
        "D=D|M\n"
        "@%s$OR_TURE.%zu\n"
        "D;JEQ\n"
        "M=0\n"
        "@%s$END_OR.%zu\n"
        "0;JMP\n"
        "(%s$OR_TURE.%zu)\n"
        "M=-1\n"    
        "(%s$END_OR.%zu)\n",
            scope->func_name, scope->counter, scope->func_name, scope->counter,
            scope->func_name, scope->counter, scope->func_name, scope->counter
    );

    return 0;
}

static int logical_not(FILE* out)
{
    fprintf(out,
        "@SP\n"
        "AM=M-1\n"
        "M=!M\n"
    );

    return 0;
}

static int label(FILE* out, char* label_name)
{
    fprintf(out, 
        "(%s)\n",label_name);;
        
    return 0;
}

static int __goto(FILE* out, char* label_name)
{  
    fprintf(out,
        "@%s\n" \
        "0;JMP\n", label_name);

    return 0;
}

static int if_goto(FILE* out, char* label_name)
{
    fprintf(out,
        "@SP\n" \
        "AM=M-1\n" \
        "D=M\n" \
        "@%s\n" \
        "D;JGT\n", label_name);

    return 0;
}

static int function_declaration(FILE* out, char* func_name, int nVars)
{   
    // create the label for start of the function_declaration.
    size_t dest_size = strlen(func_name) + function_declaration_label_template_len + 1;

    char* funcAddrLablel = templated_label_creator(dest_size, function_declaration_label_template, func_name);
    if (!funcAddrLablel) return UNEXPECTED_ERR;

    // put a lable for function
    label(out, funcAddrLablel);

    free(funcAddrLablel);

    // init local variables
    for (int i = 0; i < nVars; i++)
    {
        push_constant(out, 0);
    }

    return 0;
}

static int function_call(FILE* out, char* func_name, int nArgs, Scope* scope) {
    size_t dest_size = strlen(scope->func_name) + n_digit(scope->counter) + return_addr_label_template_len + 1;

    char* returnAddr = templated_label_creator(dest_size, return_addr_label_template,
        scope->func_name, scope->counter);
    if (!returnAddr) return UNEXPECTED_ERR;
    
    // save function return address
    fprintf(out,
        "@%s\n"             
        "D=A\n"                     
        "@SP\n"                     
        "A=M\n"                     
        "M=D\n"                     
        "@SP\n"                     
        "M=M+1\n", returnAddr);
    
    // save current LCL, ARG, THIS, THAT
    fprintf(out,                  
        "@LCL\n"
        "D=M\n"
        "@SP\n"
        "A=M\n"
        "M=D\n"
        "@SP\n"                     
        "M=M+1\n"
        "@ARG\n"
        "D=M\n"
        "@SP\n"
        "A=M\n"
        "M=D\n"
        "@SP\n"                     
        "M=M+1\n"
        "@THIS\n"
        "D=M\n"
        "@SP\n"
        "A=M\n"
        "M=D\n"
        "@SP\n"                     
        "M=M+1\n"
        "@THAT\n"
        "D=M\n"
        "@SP\n"
        "A=M\n"
        "M=D\n"
        "@SP\n"                     
        "M=M+1\n");

    int arg_offset = 5 + nArgs;

    // set ARG pointer (ARG = SP - (nArgs + 5))
    fprintf(out,
        "@SP\n"
        "D=M\n"
        "@%d\n"
        "D=D-A\n"
        "@ARG\n"
        "M=D\n", arg_offset
    );

    // reposition LCL
    fprintf(out,
        "@SP\n"
        "D=M\n"
        "@LCL\n"
        "M=D\n"
    );

    // create the calling function label for goto.
    dest_size = strlen(func_name) + function_declaration_label_template_len + 1;

    char* funcAddrLablel = templated_label_creator(dest_size, function_declaration_label_template, func_name);
        if (!funcAddrLablel) {
        free(returnAddr);
        return UNEXPECTED_ERR;
    }

    // goto function
    __goto(out, funcAddrLablel);

    // label for returnAddr for return command.
    label(out, returnAddr);
    
    free(returnAddr);
    free(funcAddrLablel);

    return 0;
}

static int function_return(FILE* out)
{   
    // store frame address in R13 temp register.
    fprintf(out,
        "@LCL\n"
        "D=M\n"
        "@R13\n"
        "M=D\n"
    );

    // store return address in R14 temp register.
    fprintf(out,
        "@5\n"
        "A=D-A\n"
        "D=M\n"
        "@R14\n"
        "M=D\n"
    );    

    // pop top of stack into the arg0. (return value)
    pop(out, "ARG", 0);

    // reposition SP to point the the arg + 1.
    fprintf(out,
        "@ARG\n"
        "D=M\n"
        "D=D+1\n"
        "@SP\n"
        "M=D\n"
    );

    // reset THAT
    fprintf(out,
        "@R13\n"
        "A=M-1\n"
        "D=M\n"
        "@THAT\n"
        "M=D\n"
    );

    // reset THIS
    fprintf(out,
        "@2\n"
        "D=A\n"
        "@R13\n"
        "A=M-D\n"
        "D=M\n"
        "@THIS\n"
        "M=D\n"
    );

    // reset ARG
    fprintf(out,
        "@3\n"
        "D=A\n"
        "@R13\n"
        "A=M-D\n"
        "D=M\n"
        "@ARG\n"
        "M=D\n"
    );

    // reset LCL
    fprintf(out,
        "@4\n"
        "D=A\n"
        "@R13\n"
        "A=M-D\n"
        "D=M\n"
        "@LCL\n"
        "M=D\n"
    );

    // jump to return address.
    fprintf(out,
        "@R14\n"
        "A=M\n"
        "0;JMP\n");

    return 0;
}

static Scope* add_scope_to_tail(Scope* tail, const char* func_name)
{
    Scope* new_tail = (Scope*)malloc(sizeof(Scope));
    if (!new_tail) return NULL;

    new_tail->func_name = func_name;
    new_tail->counter = 1;

    new_tail->previous = tail;
    new_tail->next = NULL;
    
    if (tail) tail->next = new_tail;

    return new_tail;
}

static Scope* remove_scope_from_tail(Scope* tail)
{
    if (tail == NULL) return NULL;

    Scope* new_tail = tail->previous;
    if (new_tail != NULL) 
    {   
        new_tail->next = NULL;
    }

    free(tail);
    return new_tail;
}

size_t n_digit(size_t num)
{   
    size_t len = 1;
   
    while (num >= 10)
    {   
        num /= 10;
        len++;
    }

    return len;
}

static char* templated_label_creator(size_t dest_size, const char* fmt, ...)
{
   char* dest = (char*) malloc(dest_size);
   if (!dest) return NULL;

    va_list ap;
    va_start(ap, fmt);

    vsnprintf(dest, dest_size, fmt, ap);

    return dest;
}

static int temp_mmap(int offset)
{
    if (offset < 0 || offset > 7)
        return UNEXPECTED_ERR;

    return offset + 5;
}