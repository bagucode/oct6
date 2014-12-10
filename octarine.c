// Compile debug:   clang -D DEBUG -O0 -g -o octarine octarine.c
// Compile release: clang -D RELEASE -Ofast -o octarine octarine.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// TODO: Error type
// TODO: make interpreter functions return void, should use stack!

// Runtime types
typedef struct sRuntime Runtime;
typedef struct sContext Context;
typedef struct sEnvironment Environment;
typedef struct sType Type;
typedef struct sObject Object;
typedef struct sStack Stack;
typedef Object* (*BuiltInFn)(Context* ctx);
typedef struct sError Error;
typedef struct sStream Stream;
typedef enum eStreamType StreamType;
typedef struct sTokenizer Tokenizer;
typedef struct sReader Reader;

// Language types (Move more runtime types here. Full reflection is nice.)
typedef struct sNumber Number;
typedef struct sSymbol Symbol;
typedef struct sList List;
typedef struct sFunction Function;

// Runtime type definitions
struct sObject {
  Type* type;
  Object* next;
  unsigned int marked;
  char data[0];
};

struct sType {
  unsigned int size;
  unsigned int alignment;
  char* name;
  Function* deleteFn;
  Function* printFn;
  Function* evalFn;
  unsigned int nFields;
  Type** fields;
};

struct sStack {
  unsigned int size;
  unsigned int top;
  Object** data;
};

struct sRuntime {
  unsigned int nContexts;
  unsigned int contextListSize;
  Context** contexts;
  Context* currentContext;
  Environment* environment;
};

struct sContext {
  Runtime* runtime;
  Environment* environment;
  Stack* stack;
  Object* lastObject;
  Reader* reader;
};

struct sEnvironment {
  unsigned int bindingsListSize;
  char** names;
  Object** objects;
  Environment* parent;
};

struct sError {
  char* message;
};

enum eStreamType {
  ST_STRING,
  ST_FILE
};

struct sStream {
  StreamType type;
  union {
    struct {
      unsigned int stringPos;
      char* string;
    };
    FILE* file;
  };
};

struct sTokenizer {
  char c;
  unsigned int tokenSize;
  char* token;
  Stream* stream;
};

struct sReader {
  Tokenizer* tokenizer;
};

// Language type definitions

struct sNumber {
  double value;
};

struct sSymbol {
  char* name;
};

struct sList {
  Object* value;
  Object* next;
};

struct sFunction {
  char* name;
  char isBuiltIn;
  union {
    BuiltInFn builtIn;
    Object* code;
  };
};

// All of globals

static Type tNumber;
static Type tSymbol;
static Type tList;
static Type tFunction;

// All of functions

static Stream* StreamNew(StreamType type, const char* strOrFileName) {
  Stream* s = (Stream*)malloc(sizeof(Stream));
  if(!s) {
    goto cleanup;
  }

  s->type = type;

  if(type == ST_STRING) {
    unsigned int len = strlen(strOrFileName);
    s->stringPos = 0;
    s->string = (char*)malloc(len + 1);
    if(!s->string) {
      goto cleanup;
    }
    memcpy(s->string, strOrFileName, len);
    s->string[len] = 0;
  }
  else if(type == ST_FILE) {
    s->file = fopen(strOrFileName, "rb");
    if(!s->file) {
      goto cleanup;
    }
  }
  else {
    goto cleanup;
  }

  goto end;

 cleanup:
  free(s);
  s = NULL;

 end:
  return s;
}

static void StreamDelete(Stream* stream) {
  if(!stream) {
    return;
  }

  if(stream->type == ST_STRING) {
    free(stream->string);
  }
  else if(stream->type == ST_FILE) {
    fclose(stream->file);
  }

  free(stream);
}

static Tokenizer* TokenizerNew(StreamType inputType, const char* strOrFileName) {
  Tokenizer* t = (Tokenizer*)malloc(sizeof(Tokenizer));
  if(!t) {
    goto cleanup;
  }

  t->c = ' ';
  t->tokenSize = 100;

  t->token = (char*)malloc(t->tokenSize);
  if(!t->token) {
    goto cleanup;
  }
  t->token[0] = 0;

  t->stream = StreamNew(inputType, strOrFileName);
  if(!t->stream) {
    goto cleanup;
  }

  goto end;

 cleanup:
  if(t) {
    free(t->token);
    free(t);
    t = NULL;
  }

 end:
  return t;
}

static void TokenizerDelete(Tokenizer* tokenizer) {
  if(!tokenizer) {
    return;
  }

  StreamDelete(tokenizer->stream);
  free(tokenizer->token);
  free(tokenizer);
}

static Reader* ReaderNew(StreamType inputType, const char* strOrFileName) {
  Reader* r = (Reader*)malloc(sizeof(Reader));
  if(!r) {
    return NULL;
  }

  r->tokenizer = TokenizerNew(inputType, strOrFileName);
  if(!r->tokenizer) {
    free(r);
    return NULL;
  }

  return r;
}

static void ReaderDelete(Reader* reader) {
  if(!reader) {
    return;
  }

  TokenizerDelete(reader->tokenizer);
  free(reader);
}

static Environment* EnvironmentNew(Environment* parent) {
  Environment* env = (Environment*)malloc(sizeof(Environment));
  if(!env) {
    goto cleanup;
  }

  env->parent = parent;
  env->bindingsListSize = 100;

  env->names = (char**)malloc(sizeof(char*) * env->bindingsListSize);
  if(!env->names) {
    goto cleanup;
  }

  for(unsigned int i = 0; i < env->bindingsListSize; ++i) {
    env->names[i] = NULL; // free slot
  }

  env->objects = (Object**)malloc(sizeof(Object*) * env->bindingsListSize);
  if(!env->objects) {
    goto cleanup;
  }

  goto end;

 cleanup:
  if(env) {
    free(env->names);
    free(env);
    env = NULL;
  }

 end:
  return env;
}

static void EnvironmentDelete(Environment* env) {
  if(!env) {
    return;
  }

  for(unsigned int i = 0; i < env->bindingsListSize; ++i) {
    free(env->names[i]);
  }

  free(env->names);
  free(env->objects);
  free(env);
}

static Stack* StackNew() {
  Stack* s = (Stack*)malloc(sizeof(Stack));
  if(!s) {
    return NULL;
  }

  s->size = 1000;
  s->top = 0;
  s->data = (Object**)malloc(sizeof(Object*) * s->size);
  if(!s->data) {
    free(s);
    return NULL;
  }
  return s;
}

static void StackDelete(Stack* s) {
  if(!s) {
    return;
  }

  free(s->data);
  free(s);
}

static Context* ContextNew(Runtime* rt, StreamType inputType, const char* strOrFileName) {
  Context* ctx = (Context*)malloc(sizeof(Context));
  if(!ctx) {
    return NULL;
  }
  ctx->runtime = rt;
  ctx->lastObject = NULL;
  ctx->stack = NULL;

  ctx->environment = EnvironmentNew(rt->environment);
  if(!ctx->environment) {
    goto cleanup;
  }

  ctx->stack = StackNew();
  if(!ctx->stack) {
    goto cleanup;
  }

  ctx->reader = ReaderNew(inputType, strOrFileName);
  if(!ctx->reader) {
    goto cleanup;
  }

  goto end;

 cleanup:
  if(ctx) {
    EnvironmentDelete(ctx->environment);
    StackDelete(ctx->stack);
    ctx = NULL;
  }

 end:
  return ctx;
}

static void StackPush(Stack* s, Object* value) {
  if(s->top == s->size) {
    unsigned int newSize = s->size;
    Object** newData = (Object**)realloc(s->data, sizeof(Object*) * newSize);
    if(!newData) {
      // TODO: return error here instead.
      fputs("realloc failed", stderr);
      abort();
    }
    s->size = newSize;
    s->data = newData;
  }
  s->data[s->top++] = value;
}

static void ObjectDelete(Context* ctx, Object* o) {
  if(o->type->deleteFn) {
    StackPush(ctx->stack, o);
    o->type->deleteFn->builtIn(ctx);
  }
}

static void ContextDelete(Context* ctx) {
  if(!ctx) {
    return;
  }

  Object* current = ctx->lastObject;
  while(current) {
    Object* next = current->next;
    ObjectDelete(ctx, current);
    free(current);
    current = next;
  }

  EnvironmentDelete(ctx->environment);
  StackDelete(ctx->stack);
  ReaderDelete(ctx->reader);
  free(ctx);
}

static Object* StackPop(Stack* s) {
  if(s->top == 0) {
    // TODO: Error here!
    return NULL;
  }

  return s->data[--s->top];
}

static unsigned long long alignOffset(unsigned long long offset, unsigned long long on) {
  assert(sizeof(unsigned long long) >= sizeof(void*));
  return (offset + (on - 1)) & (~(on - 1));
}

static void* ObjectGetDataPtr(Object* o) {
  unsigned long long offset = (unsigned long long) &o->data[0];
  unsigned long long dataLocation = alignOffset(offset, o->type->alignment);
  return (void*) dataLocation;
}

static int SymbolP(Object* o) {
  return o->type == &tSymbol;
}

static Object* SymbolDelete(Context* ctx) {
  Object* o = StackPop(ctx->stack);
  if(!SymbolP(o)) {
    abort(); // TODO: return error
  }
  Symbol* s = ObjectGetDataPtr(o);
  free(s->name);
  return NULL;
}

static Object* SymbolPrint(Context* ctx) {
  Object* o = StackPop(ctx->stack);
  if(!SymbolP(o)) {
    abort(); // TODO: return error
  }
  Symbol* s = ObjectGetDataPtr(o);
  fputs(s->name, stdout);
  return NULL;
}

static Object* EnvironmentGet(Context* ctx, Symbol* name);

static Object* SymbolEval(Context* ctx) {
  Object* o = StackPop(ctx->stack);
  if(!SymbolP(o)) {
    abort(); // TODO: error
  }
  Symbol* s = ObjectGetDataPtr(o);
  return EnvironmentGet(ctx, s);
}

static Function fSymbolDelete;
static Function fSymbolPrint;
static Function fSymbolEval;

static int NumberP(Object* o) {
  return o->type == &tNumber;
}

static Object* NumberPrint(Context* ctx) {
  Object* o = StackPop(ctx->stack);
  if(!NumberP(o)) {
    abort(); // TODO: return error
  }
  Number* n = ObjectGetDataPtr(o);
  printf("%f", n->value);
  return NULL;
}

static Function fNumberPrint;

static int ListP(Object* o) {
  return o->type == &tList;
}

static Object* ListPrint(Context* ctx) {
  Object* o = StackPop(ctx->stack);
  if(!ListP(o)) {
    abort(); // TODO: return error
  }
  List* l = ObjectGetDataPtr(o);
  fputc('(', stdout);
  while(l->value) {
    if(l->value->type->printFn && l->value->type->printFn->isBuiltIn) {
      StackPush(ctx->stack, l->value);
      l->value->type->printFn->builtIn(ctx);
      if(l->next && ListP(l->next) && ((List*)ObjectGetDataPtr(l->next))->value) {
        fputc(' ', stdout);
      }
    }
    if(!l->next) {
      break;
    }
    o = l->next;
    if(!ListP(o)) {
      abort(); // TODO: return error
    }
    l = ObjectGetDataPtr(o);
  }
  fputc(')', stdout);
  return NULL;
}

static Function fListPrint;

static int FunctionP(Object* o) {
  return o->type == &tFunction;
}

static Object* FunctionPrint(Context* ctx) {
  Object* o = StackPop(ctx->stack);
  if(!FunctionP(o)) {
    abort(); // TODO: return error
  }
  Function* f = ObjectGetDataPtr(o);
  printf("#<Function [%s]>", f->name);
  return NULL;
}

static Function fFunctionPrint;

static void initBuiltins() {
  // TODO: make thread safe
  static int initDone = 0;
  if(initDone) {
    return;
  }
  initDone = 1;

  // Number

  tNumber.alignment = sizeof(double);
  tNumber.nFields = 0;
  tNumber.size = sizeof(Number);
  tNumber.fields = NULL;
  tNumber.name = "Number";

  tNumber.deleteFn = NULL;
  tNumber.evalFn = NULL;

  fNumberPrint.name = "number-print";
  fNumberPrint.isBuiltIn = 1;
  fNumberPrint.builtIn = &NumberPrint;
  tNumber.printFn = &fNumberPrint;

  // Symbol

  tSymbol.alignment = sizeof(void*);
  tSymbol.nFields = 0;
  tSymbol.size = sizeof(Symbol);
  tSymbol.fields = NULL;
  tSymbol.name = "Symbol";

  fSymbolDelete.name = "symbol-delete";
  fSymbolDelete.isBuiltIn = 1;
  fSymbolDelete.builtIn = &SymbolDelete;
  tSymbol.deleteFn = &fSymbolDelete;

  fSymbolPrint.name = "symbol-print";
  fSymbolPrint.isBuiltIn = 1;
  fSymbolPrint.builtIn = &SymbolPrint;
  tSymbol.printFn = &fSymbolPrint;

  fSymbolEval.name = "symbol-eval";
  fSymbolEval.isBuiltIn = 1;
  fSymbolEval.builtIn = &SymbolEval;
  tSymbol.evalFn = &fSymbolEval;

  // List

  tList.alignment = sizeof(void*);
  tList.nFields = 0;
  tList.size = sizeof(List);
  tList.fields = NULL;
  tList.name = "List";

  tList.deleteFn = NULL;
  tList.evalFn = NULL;

  fListPrint.name = "list-print";
  fListPrint.isBuiltIn = 1;
  fListPrint.builtIn = &ListPrint;
  tList.printFn = &fListPrint;

  // Function

  tFunction.alignment = sizeof(void*);
  tFunction.nFields = 0;
  tFunction.size = sizeof(Function);
  tFunction.fields = NULL;
  tFunction.name = "Function";

  tFunction.deleteFn = NULL;
  tFunction.evalFn = NULL;

  fFunctionPrint.name = "function-print";
  fFunctionPrint.isBuiltIn = 1;
  fFunctionPrint.builtIn = &FunctionPrint;
  tFunction.printFn = &fFunctionPrint;
}

static Runtime* RuntimeNew(StreamType inputType, const char* strOrFileName) {
  Runtime* rt = (Runtime*)malloc(sizeof(Runtime));
  if(!rt) {
    goto cleanup;
  }

  initBuiltins();

  rt->nContexts = 1;
  rt->contextListSize = 100;
  rt->contexts = NULL;

  rt->environment = EnvironmentNew(NULL);
  if(!rt->environment) {
    goto cleanup;
  }

  rt->contexts = (Context**)malloc(sizeof(Context*) * rt->contextListSize);
  if(!rt->contexts) {
    goto cleanup;
  }

  for(unsigned int i = 0; i < rt->contextListSize; ++i) {
    rt->contexts[i] = NULL;
  }

  rt->currentContext = ContextNew(rt, inputType, strOrFileName);
  if(!rt->currentContext) {
    goto cleanup;
  }

  rt->contexts[0] = rt->currentContext;

  goto end;

 cleanup:
  if(rt) {
    free(rt->contexts);
    EnvironmentDelete(rt->environment);
    free(rt);
    rt = NULL;
  }

 end:
  return rt;
}

static void RuntimeDelete(Runtime* rt) {
  if(!rt) {
    return;
  }

  for(unsigned int i = 0; i < rt->contextListSize; ++i) {
    ContextDelete(rt->contexts[i]);
  }
  free(rt->contexts);

  EnvironmentDelete(rt->environment);
  free(rt);
}

static int StreamEnd(Stream* s) {
  if(s->type == ST_STRING) {
    return s->string[s->stringPos] == 0;
  }
  else if(s->type == ST_FILE) {
    return feof(s->file);
  }
  abort();
}

// Returns a value between 0 and 255, or -1 on end of input.
static int StreamGet(Stream* s) {
  if(StreamEnd(s)) {
    return -1;
  }
  if(s->type == ST_STRING) {
    return s->string[s->stringPos++];
  }
  else if(s->type == ST_FILE) {
    return fgetc(s->file);
  }
  abort();
}


// Returns the next token or NULL on end of input.
static const char* TokenizerNext(Tokenizer* tokenizer) {
  const char ws[] = " \n\r\t\v\b\f"; // 7
  const char delims[] = "()[]{}"; // 6

  if(StreamEnd(tokenizer->stream)) {
    return NULL;
  }

  unsigned int pos = 0;
  while(1) {
    if(tokenizer->c == -1) {
      if(pos == 0) {
        return NULL;
      }
      goto end;
    }

    for(unsigned int i = 0; i < 7; ++i) {
      if(tokenizer->c == ws[i]) {
        if(pos == 0) {
          goto nextChar;
        }
        else {
          tokenizer->c = StreamGet(tokenizer->stream);
          goto end;
        }
      }
    }

    for(unsigned int i = 0; i < 6; ++i) {
      if(tokenizer->c == delims[i]) {
        if(pos != 0) {
          goto end;
        }
        else {
          tokenizer->token[pos++] = tokenizer->c;
          tokenizer->c = StreamGet(tokenizer->stream);
          goto end;
        }
      }
    }

    if(pos == tokenizer->tokenSize) {
      unsigned int newSize = tokenizer->tokenSize * 2;
      char* newToken = (char*)realloc(tokenizer->token, newSize);
      if(newToken == NULL) {
        // TODO: return error here instead.
        fputs("realloc failed", stderr);
        abort();
      }
      tokenizer->tokenSize = newSize;
      tokenizer->token = newToken;
    }

    tokenizer->token[pos++] = tokenizer->c;
  nextChar:
    tokenizer->c = StreamGet(tokenizer->stream);
  }

 end:
  tokenizer->token[pos] = 0;
  return tokenizer->token;
}

static Object* ObjectAllocRaw(Context* ctx, Type* type) {
  Object* o = malloc(sizeof(Object) + sizeof(void*) + type->alignment - 1 + type->size);
  if(!o) {
    return NULL;
  }

  o->type = type;
  o->next = ctx->lastObject;
  o->marked = 0;

  ctx->lastObject = o;

  return o;
}

static Object* ReaderReadInternal(Context* ctx, Reader* r);

static Object* readNumber(Context* ctx, const char* token) {
  Object* result;
  char* endptr;
  double number = strtod(token, &endptr);
  if(endptr > token) {
    result = ObjectAllocRaw(ctx, &tNumber);
    if(!result) {
      abort(); // TODO: return error
    }
    Number* n = ObjectGetDataPtr(result);
    n->value = number;
    return result;
  }
  return NULL;
}

static Object* readList(Context* ctx, const char* token, Reader* r) {
  if(strcmp(token, "(") != 0) {
    return NULL;
  }
  token = TokenizerNext(r->tokenizer);
  if(!token) {
    return NULL;
  }
  Object* headObj = ObjectAllocRaw(ctx, &tList);
  if(!headObj) {
    abort(); // TODO: return error
  }
  List* lst = ObjectGetDataPtr(headObj);
  lst->value = NULL;
  lst->next = NULL;
  while(strcmp(token, ")")) {
    Object* value = ReaderReadInternal(ctx, r);
    if(!value) {
      return NULL; // TODO: return error; premature end of input
    }
    if(lst->value) {
      lst->next = ObjectAllocRaw(ctx, &tList);
      if(!lst->next) {
        abort(); // TODO: return error
      }
      lst = ObjectGetDataPtr(lst->next);
      lst->value = NULL;
      lst->next = NULL;
    }
    lst->value = value;
    token = TokenizerNext(r->tokenizer);
    if(!token) {
      return NULL;
    }
  }
  return headObj;
}

static Object* SymbolNew(Context* ctx, const char* name) {
  Object* symObj = ObjectAllocRaw(ctx, &tSymbol);
  if(!symObj) {
    abort(); // TODO: return error
  }
  Symbol* sym = ObjectGetDataPtr(symObj);
  unsigned int len = strlen(name);
  sym->name = malloc(len + 1);
  if(!sym->name) {
    abort(); // TODO: return error
  }
  memcpy(sym->name, name, len);
  sym->name[len] = 0;
  return symObj;
}

static Object* readSymbol(Context* ctx, const char* token) {
  return SymbolNew(ctx, token);
}

static Object* ReaderReadInternal(Context* ctx, Reader* r) {
  const char* token = r->tokenizer->token;
  Object* result = readNumber(ctx, token);
  if(!result) {
    result = readList(ctx, token, r);
  }
  if(!result) {
    result = readSymbol(ctx, token);
  }

  return result;
}

// Returns NULL on end of input
static Object* ReaderRead(Context* ctx, Reader* r) {
  const char* token = TokenizerNext(r->tokenizer);
  if(!token) {
    return NULL;
  }
  return ReaderReadInternal(ctx, r);
}

static Object* EnvironmentGet(Context* ctx, Symbol* name) {
  for(unsigned int i = 0; i < ctx->environment->bindingsListSize; ++i) {
    if(ctx->environment->names[i] && strcmp(ctx->environment->names[i], name->name) == 0) {
      return ctx->environment->objects[i];
    }
  }
  return NULL;
}

// Returns previous value, or NULL if none
static Object* EnvironmentBind(Context* ctx, Symbol* name, Object* obj) {
  unsigned int freeSlot = 0;
  char hasFree = 0;
  for(unsigned int i = 0; i < ctx->environment->bindingsListSize; ++i) {
    if(!hasFree && ctx->environment->names[i] == NULL) {
      hasFree = 1;
      freeSlot = i;
    }
    if(ctx->environment->names[i] &&
       strcmp(ctx->environment->names[i], name->name) == 0) {
      Object* previous = ctx->environment->objects[i];
      ctx->environment->objects[i] = obj;
      return previous;
    }
  }
  if(!hasFree) {
    unsigned int newBindingsSize = ctx->environment->bindingsListSize * 2;
    char** newNames = realloc(ctx->environment->names, newBindingsSize * sizeof(char*));
    if(!newNames) {
      abort(); // TODO: error
    }
    for(unsigned int i = ctx->environment->bindingsListSize; i < newBindingsSize; ++i) {
      newNames[i] = NULL;
    }
    Object** newObjects = realloc(ctx->environment->objects, newBindingsSize * sizeof(Object*));
    if(!newObjects) {
      abort(); // TODO: error
    }
    hasFree = 1;
    freeSlot = ctx->environment->bindingsListSize;
    ctx->environment->bindingsListSize = newBindingsSize;
    ctx->environment->names = newNames;
    ctx->environment->objects = newObjects;
  }
  unsigned int nameLen = strlen(name->name);
  ctx->environment->names[freeSlot] = malloc(nameLen + 1);
  if(!ctx->environment->names[freeSlot]) {
    abort(); // TODO: error
  }
  memcpy(ctx->environment->names[freeSlot], name->name, nameLen);
  ctx->environment->names[freeSlot][nameLen] = 0;
  ctx->environment->objects[freeSlot] = obj;
  return NULL;
}

// Entry point

int main(int argc, char* argv[]) {
  if(argc < 2) {
    fputs("Give program please.\n", stderr);
    return -1;
  }
  Runtime* rt = RuntimeNew(ST_FILE, argv[1]);
  if(!rt) {
    fputs("Give program please.\n", stderr);
    return -1;
  }

#ifdef DEBUG
  puts("octarine 0.0.1, debug build");
#elif defined RELEASE
  puts("octarine 0.0.1, release build");
#else
#error Must define DEBUG or RELEASE
#endif

  Context* ctx = rt->currentContext;
  Reader* r = ctx->reader;
  Object* o = ReaderRead(ctx, r);
  while(o) {
    if(o->type->evalFn && o->type->evalFn->isBuiltIn) {
      StackPush(ctx->stack, o);
      o = o->type->evalFn->builtIn(ctx);
    }
    if(!o) {
      puts("nil");
    }
    else if(o->type->printFn && o->type->printFn->isBuiltIn) {
      StackPush(ctx->stack, o);
      o->type->printFn->builtIn(ctx);
      putc('\n', stdout);
    }
    o = ReaderRead(ctx, r);
  }

  RuntimeDelete(rt);
  return 0;
}
