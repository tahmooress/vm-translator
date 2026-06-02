#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include "./parser.h"
#include "./code_writer.h"

char* get_file_name_without_extention(const char* filepath, const char* ext);
char* add_file_extention(const char* filename, const char* ext);

int main(int argc, char *argv[])
{   
    if (argc < 2) 
    {
        fprintf(stderr, "path to the .vm file not specifiy");
        exit(1);
    }

    char *filename = argv[1];

    if (!filename || !*filename) {
        fprintf(stderr, "empty filename :%s",filename);
        exit(1);
    }

    if (isupper((unsigned char)*filename) != 0) {
        fprintf(stderr, "filename should start with uppercase letter:%s",filename);
        exit(1);
    }

    const char *ext = ".vm";
    size_t ext_size = strlen(ext);
    size_t file_size = strlen(filename);
    
    if (file_size < ext_size)
    {   
        fprintf(stderr, "Err: input file should have .vm extention");
        exit(1);
    }

    if (strcmp(filename + (file_size - ext_size), ext))
    {
        fprintf(stderr, "Err: input file should have .vm extention");
        exit(1);
    }

    FILE* input = fopen(filename, "r");
    if(!input) {
        fprintf(stderr, "cant open file :%s",filename);
        exit(1);
    }

    char* output_file_name_without_extention = get_file_name_without_extention(filename, ".vm");
    if (!output_file_name_without_extention) {
        fprintf(stderr, "trimming filename extention failed\n");
        exit(-1);
    }

    char* output_file_name = add_file_extention(output_file_name_without_extention, ".asm");
    if (!output_file_name) {
        fprintf(stderr, "adding file-extention failed\n");
        free(output_file_name_without_extention);
        exit(-1);
    }

    Parser* parser = init_parser(input);
        if(!parser) {
        fprintf(stderr, "failed to init_parser");
        fclose(input);
        free(output_file_name_without_extention);
        free(output_file_name);

        exit(1);
    }

    FILE* output = fopen(output_file_name, "w");
    if (!output) {
        fprintf(stderr, "cant open output file: %s", output_file_name);
        fclose(input);
        free(output_file_name_without_extention);
        free(output_file_name);
        free_parser(parser);
        
        exit(1);
    }

    CodeWriter* codeWriter = init_code_writer(output, output_file_name_without_extention, VM_ANNOTATE|VM_DEBUG);
    Statement* stm;

    int err;

    while((stm = next_statement(parser)) != NULL) {
        err = write_code(codeWriter, stm);
        if (err) {
            free_code_writer(codeWriter);
            fclose(output);
            fclose(input);
            free(output_file_name_without_extention);
            free(output_file_name);
            free_parser(parser);
            free_statement(stm);
            exit(1);
        }
    }
    
    write_halt(output);

    free_parser(parser);
    free(codeWriter);
    free(output_file_name_without_extention);
    free(output_file_name);
    fclose(input);
    fclose(output);
    exit(0);
}

char* get_file_name_without_extention(const char* filepath, const char* ext)
{
    if (!filepath || !*filepath) {
        return NULL;
    }

    const char* end = filepath;

    while(*end) end++;

    const char* start = end;
    while(start > filepath && *start != '/')  start--;

    if (*start == '/') start++;
    
    end = strstr(start, ext);

    size_t len = end - start +1;
    char* str = (char*) malloc(len);
    if (!str) return NULL;

    char* filename = str;

    while(len-- > 1) {
        *str++ = *start++;
    }

    *str = '\0';

    return filename;
}

char* add_file_extention(const char* filename, const char* ext)
{   
    if (!filename || !*filename) {
        return NULL;
    }

    size_t str_len = strlen(filename);
    size_t ext_len = strlen(ext);

    char* str = (char*) malloc(str_len + ext_len + 1);
    if (!str) return NULL;

    char* start = str;

    while(filename && *filename) *str++ = *filename++;

    while(ext && *ext) *str++ = *ext++;

    *str = '\0';

    return start;
}