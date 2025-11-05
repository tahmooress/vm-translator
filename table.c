#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TABLE_SIZE 256

typedef struct TokenSymbol {
    char   *key;           
    void   *val;          
    struct TokenSymbol *next;
} TokenSymbol;

typedef struct {
    TokenSymbol *hash_table[MAX_TABLE_SIZE];
} TokenSymbolTable;


static unsigned int hash_key(const char *s) {
    unsigned long h = 5381;
    int c;
    while ((c = (unsigned char)*s++)) {
        h = ((h << 5) + h) + c; // h*33 + c
    }
    return (unsigned int)(h % MAX_TABLE_SIZE);
}

TokenSymbolTable *token_symbol_table_init(void) {
    TokenSymbolTable *st = calloc(1, sizeof *st); // zero-initialized
    return st;
}


int token_symbol_table_set(TokenSymbolTable *t, const char *key, void *val, size_t val_size) {
    if (!t || !key) return 1;

    unsigned int idx = hash_key(key);
    for (TokenSymbol *n = t->hash_table[idx]; n; n = n->next) {
        if (strcmp(n->key, key) == 0) {
            n->val = val; // update in place
            return 1;
        }
    }
    // new node (own the key)
    TokenSymbol *n = malloc(sizeof *n);
    if (!n) return 0;
    n->key = strdup(key);
    if (!n->key) { 
        free(n);
         
        return 1;
    }

    void* cpVal = malloc(val_size);
    if (!cpVal) {
        free(n);


        return 1;
    }

    memcpy(cpVal, val, val_size);


    n->val = cpVal;
    n->next = t->hash_table[idx];
    t->hash_table[idx] = n;

    return 0;
}

/* Get: return 1 if found and write to *out, else 0 */
int token_symbol_table_get(TokenSymbolTable *t, const char *key, void** const out) {
    if (!t || !key) return 0;
    unsigned int idx = hash_key(key);
    for (TokenSymbol *n = t->hash_table[idx]; n; n = n->next) {
        if (strcmp(n->key, key) == 0) {
            if (out) *out = n->val;
            return 1;
        }
    }
    return 0;
}

/* Delete: removes entry; if out!=NULL, writes old value. Returns 1 if existed. */
int token_symbol_table_delete(TokenSymbolTable *t, const char *key, void** const out) {
    if (!t || !key) return 0;
    unsigned int idx = hash_key(key);

    TokenSymbol *prev = NULL;
    TokenSymbol *cur  = t->hash_table[idx];
    while (cur && strcmp(cur->key, key) != 0) {
        prev = cur;
        cur = cur->next;
    }
    if (!cur) return 0;

    if (out) *out = cur->val;

    if (prev) prev->next = cur->next;
    else      t->hash_table[idx] = cur->next;

    free(cur->key);
    free(cur->val);
    free(cur);
    
    return 1;
}

void token_symbol_table_free(TokenSymbolTable* t) {
    if (!t) return;

    for (int i = 0; i < MAX_TABLE_SIZE; i++) {
        TokenSymbol* current = t->hash_table[i];
        while (current) {
            TokenSymbol* next = current->next;
            free((void*)current->key);
            free(current->val);
            free(current);

            current = next;
        }
        t->hash_table[i] = NULL;
    }

    free(t);
}
