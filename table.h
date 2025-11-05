#ifndef TABLE_H
#define TABLE_H

typedef struct TokenSymbolTable TokenSymbolTable;

TokenSymbolTable *token_symbol_table_init(void);
int token_symbol_table_set(TokenSymbolTable *t, const char *key, void *val, size_t val_size);
int token_symbol_table_get(TokenSymbolTable *t, const char *key, void** const out);
int token_symbol_table_delete(TokenSymbolTable *t, const char *key, void** const out);
void token_symbol_table_free(TokenSymbolTable* t);

#endif

