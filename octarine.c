// Compile debug:   clang -D DEBUG -O0 -g -o octarine octarine.c
// Compile release: clang -D RELEASE -Ofast -o octarine octarine.c

#ifdef WIN32
#pragma warning (disable : 4996) // Stop whining about deprecated functions
#endif

#ifdef _DEBUG
#define DEBUG
#elif defined NDEBUG
#define RELEASE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <setjmp.h>

// Runtime types
typedef struct sRuntime Runtime;
typedef struct sContext Context;
typedef struct sUnwindList UnwindList;
typedef struct sEnvironment Environment;
typedef struct sType Type;
typedef struct sField Field;
typedef struct sObjectHeader ObjectHeader;
typedef struct sObject Object;
typedef struct sStack Stack;
typedef void(*BuiltInFn)(Context* ctx);
typedef struct sStream Stream;
typedef enum eStreamType StreamType;
typedef struct sTokenizer Tokenizer;
typedef struct sReader Reader;

// Language types (Move more runtime types here. Full reflection is nice.)
typedef struct sError Error;
typedef struct sNumber Number;
typedef struct sSymbol Symbol;
typedef struct sList List;
typedef struct sFunction Function;

// Runtime type definitions

struct sObjectHeader {
  unsigned int marked;
  Type* type;
  Object* next;
};

struct sObject {
  ObjectHeader header;
  char data[0];
};

struct sField {
  unsigned int offset;
  char* name;
  Type* type;
};

struct sType {
  unsigned int size;
  unsigned int alignment;
  char* name;
  unsigned int nFields;
  Field* fields;
  Function* deleteFn;
  Function* printFn;
  Function* evalFn;
  Function* applyFn;
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

struct sUnwindList {
  Object* action;
  UnwindList* next;
};

struct sContext {
  Runtime* runtime;
  Environment* environment;
  Stack* stack;
  Object* lastObject;
  unsigned int collectionThreshold;
  unsigned int bytesAllocated;
  Reader* reader;
  Object* error;
  UnwindList* unwindActions;
  jmp_buf jmpBuf;
};

struct sEnvironment {
  unsigned int bindingsListSize;
  char** names;
  Object** objects;
  Environment* parent;
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

struct sError {
  ObjectHeader _header;
  char* message;
};

struct sNumber {
  ObjectHeader _header;
  double value;
};

struct sSymbol {
  ObjectHeader _header;
  char* name;
};

struct sList {
  ObjectHeader _header;
  Object* value;
  Object* next;
};

struct sFunction {
  ObjectHeader _header;
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
static Type tError;

static Error eOOM;

// All of functions

static Object* ErrorNew(Context* ctx, const char* message);

static Stream* StreamNew(StreamType type, const char* strOrFileName) {
  Stream* s = (Stream*)malloc(sizeof(Stream));
  if (!s) {
    goto cleanup;
  }

  s->type = type;

  if (type == ST_STRING) {
    s->stringPos = 0;
    s->string = strdup(strOrFileName);
    if (!s->string) {
      goto cleanup;
    }
  }
  else if (type == ST_FILE) {
    s->file = fopen(strOrFileName, "rb");
    if (!s->file) {
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
  if (!stream) {
    return;
  }

  if (stream->type == ST_STRING) {
    free(stream->string);
  }
  else if (stream->type == ST_FILE) {
    fclose(stream->file);
  }

  free(stream);
}

static Tokenizer* TokenizerNew(StreamType inputType, const char* strOrFileName) {
  Tokenizer* t = (Tokenizer*)malloc(sizeof(Tokenizer));
  if (!t) {
    goto cleanup;
  }

  t->c = ' ';
  t->tokenSize = 100;

  t->token = (char*)malloc(t->tokenSize);
  if (!t->token) {
    goto cleanup;
  }
  t->token[0] = 0;

  t->stream = StreamNew(inputType, strOrFileName);
  if (!t->stream) {
    goto cleanup;
  }

  goto end;

cleanup:
  if (t) {
    free(t->token);
    free(t);
    t = NULL;
  }

end:
  return t;
}

static void TokenizerDelete(Tokenizer* tokenizer) {
  if (!tokenizer) {
    return;
  }

  StreamDelete(tokenizer->stream);
  free(tokenizer->token);
  free(tokenizer);
}

static Reader* ReaderNew(StreamType inputType, const char* strOrFileName) {
  Reader* r = (Reader*)malloc(sizeof(Reader));
  if (!r) {
    return NULL;
  }

  r->tokenizer = TokenizerNew(inputType, strOrFileName);
  if (!r->tokenizer) {
    free(r);
    return NULL;
  }

  return r;
}

static void ReaderDelete(Reader* reader) {
  if (!reader) {
    return;
  }

  TokenizerDelete(reader->tokenizer);
  free(reader);
}

static Environment* EnvironmentNew(Environment* parent) {
  Environment* env = (Environment*)malloc(sizeof(Environment));
  if (!env) {
    goto cleanup;
  }

  env->parent = parent;
  env->bindingsListSize = 100;

  env->names = (char**)malloc(sizeof(char*) * env->bindingsListSize);
  if (!env->names) {
    goto cleanup;
  }

  for (unsigned int i = 0; i < env->bindingsListSize; ++i) {
    env->names[i] = NULL; // free slot
  }

  env->objects = (Object**)malloc(sizeof(Object*) * env->bindingsListSize);
  if (!env->objects) {
    goto cleanup;
  }

  goto end;

cleanup:
  if (env) {
    free(env->names);
    free(env);
    env = NULL;
  }

end:
  return env;
}

static void EnvironmentDelete(Environment* env) {
  if (!env) {
    return;
  }

  for (unsigned int i = 0; i < env->bindingsListSize; ++i) {
    free(env->names[i]);
  }

  free(env->names);
  free(env->objects);
  free(env);
}

static Stack* StackNew() {
  Stack* s = (Stack*)malloc(sizeof(Stack));
  if (!s) {
    return NULL;
  }

  s->size = 1000;
  s->top = 0;
  s->data = (Object**)malloc(sizeof(Object*) * s->size);
  if (!s->data) {
    free(s);
    return NULL;
  }
  return s;
}

static void StackDelete(Stack* s) {
  if (!s) {
    return;
  }

  free(s->data);
  free(s);
}

static Context* ContextNew(Runtime* rt, StreamType inputType, const char* strOrFileName) {
  Context* ctx = (Context*)malloc(sizeof(Context));
  if (!ctx) {
    return NULL;
  }
  ctx->runtime = rt;
  ctx->lastObject = NULL;
  ctx->stack = NULL;
  ctx->error = NULL;
  ctx->unwindActions = NULL;
  ctx->bytesAllocated = 0;
  ctx->collectionThreshold = 1024 * 1024;

  ctx->environment = EnvironmentNew(rt->environment);
  if (!ctx->environment) {
    goto cleanup;
  }

  ctx->stack = StackNew();
  if (!ctx->stack) {
    goto cleanup;
  }

  ctx->reader = ReaderNew(inputType, strOrFileName);
  if (!ctx->reader) {
    goto cleanup;
  }

  goto end;

cleanup:
  if (ctx) {
    EnvironmentDelete(ctx->environment);
    StackDelete(ctx->stack);
    free(ctx->unwindActions);
    free(ctx);
    ctx = NULL;
  }

end:
  return ctx;
}

static Object* GetError(Context* ctx) {
  return ctx->error;
}

static int FunctionP(Object* o);
static void* ObjectGetDataPtr(Object* o);

static void ThrowError(Context* ctx, Object* error) {
  ctx->error = error;
  // Perform unwind actions here since they might cancel the error. I think.
  // TODO: Maybe reverse the list? Unwinding first to last is weird.
  UnwindList* ul = ctx->unwindActions;
  while (ul) {
    if (!FunctionP(ul->action)) {  // Ouch. Can't really throw here.
      fputs("FATAL: Unwind action was not a function.", stderr);
      abort();
    }
    Function* f = ObjectGetDataPtr(ul->action);
    if (f->isBuiltIn) { // TODO: invoke non-builtin
      f->builtIn(ctx);
    }
    UnwindList* tmp = ul;
    ul = ul->next;
    free(tmp);
  }
  ctx->unwindActions = NULL;
  longjmp(ctx->jmpBuf, 1);
}

static void ClearError(Context* ctx) {
  ctx->error = NULL;
}

static void ThrowOOM(Context* ctx);

static void StackPush(Context* ctx, Object* value) {
  if (ctx->stack->top == ctx->stack->size) {
    unsigned int newSize = ctx->stack->size;
    Object** newData = (Object**)realloc(ctx->stack->data, sizeof(Object*) * newSize);
    if (!newData) {
      ThrowOOM(ctx);
      return; // Not needed, ThrowOOM does longjmp but code analysis does not understand that.
    }
    ctx->stack->size = newSize;
    ctx->stack->data = newData;
  }
  ctx->stack->data[ctx->stack->top++] = value;
}

static void ObjectDelete(Context* ctx, Object* o) {
  if (o->header.type->deleteFn) {
    StackPush(ctx, o);
    o->header.type->deleteFn->builtIn(ctx);
  }
}

static void ContextDelete(Context* ctx) {
  if (!ctx) {
    return;
  }

  UnwindList* ul = ctx->unwindActions;
  while (ul) {
    UnwindList* tmp = ul;
    ul = ul->next;
    free(tmp);
  }

  Object* current = ctx->lastObject;
  while (current) {
    Object* next = current->header.next;
    ObjectDelete(ctx, current);
    free(current);
    current = next;
  }

  EnvironmentDelete(ctx->environment);
  StackDelete(ctx->stack);
  ReaderDelete(ctx->reader);
  free(ctx->unwindActions);
  free(ctx);
}

static void ThrowUnexpectedType(Context* ctx) {
  ThrowError(ctx, ErrorNew(ctx, "Unexpected type"));
}

static void ThrowOOM(Context* ctx) {
  ThrowError(ctx, (Object*)&eOOM);
}

static void ContextPushUnwindAction(Context* ctx, Object* action) {
  if (!FunctionP(action)) {
    ThrowUnexpectedType(ctx);
  }
  if (!ctx->unwindActions) {
    ctx->unwindActions = (UnwindList*)calloc(1, sizeof(UnwindList));
    if (!ctx->unwindActions) {
      ThrowOOM(ctx);
    }
    ctx->unwindActions->action = action;
    return;
  }
  UnwindList* cur = ctx->unwindActions;
  while (cur->next) {
    cur = cur->next;
  }
  cur->next = (UnwindList*)calloc(1, sizeof(UnwindList));
  if (!cur->next) {
    ThrowOOM(ctx);
  }
  cur->next->action = action;
}

static Object* StackPop(Context* ctx) {
  if (ctx->stack->top == 0) {
    ThrowError(ctx, ErrorNew(ctx, "Cannot pop empty stack"));
  }

  return ctx->stack->data[--ctx->stack->top];
}

static unsigned long long alignOffset(unsigned long long offset, unsigned long long on) {
  assert(sizeof(unsigned long long) >= sizeof(void*));
  return (offset + (on - 1)) & (~(on - 1));
}

static void* ObjectGetDataPtr(Object* o) {
  unsigned long long offset = (unsigned long long) &o->data[0];
  unsigned long long dataLocation = alignOffset(offset, o->header.type->alignment);
  return (void*)dataLocation;
}

static int SymbolP(Object* o) {
  return o->header.type == &tSymbol;
}

static void SymbolDelete(Context* ctx) {
  Object* o = StackPop(ctx);
  if (!SymbolP(o)) {
    ThrowUnexpectedType(ctx);
  }
  Symbol* s = ObjectGetDataPtr(o);
  free(s->name);
}

static void SymbolPrint(Context* ctx) {
  Object* o = StackPop(ctx);
  if (!SymbolP(o)) {
    ThrowUnexpectedType(ctx);
  }
  Symbol* s = ObjectGetDataPtr(o);
  fputs(s->name, stdout);
}

static Object* EnvironmentGet(Context* ctx, Symbol* name);

static void SymbolEval(Context* ctx) {
  Object* o = StackPop(ctx);
  if (!SymbolP(o)) {
    ThrowUnexpectedType(ctx);
  }
  Symbol* s = ObjectGetDataPtr(o);
  Object* result = EnvironmentGet(ctx, s);
  StackPush(ctx, result);
}

static Function fSymbolDelete;
static Function fSymbolPrint;
static Function fSymbolEval;

static int NumberP(Object* o) {
  return o->header.type == &tNumber;
}

static void NumberPrint(Context* ctx) {
  Object* o = StackPop(ctx);
  if (!NumberP(o)) {
    ThrowUnexpectedType(ctx);
  }
  Number* n = ObjectGetDataPtr(o);
  printf("%f", n->value);
}

static Function fNumberPrint;

static int ListP(Object* o) {
  return o->header.type == &tList;
}

static void ListPrint(Context* ctx) {
  Object* o = StackPop(ctx);
  if (!ListP(o)) {
    ThrowUnexpectedType(ctx);
  }
  List* l = ObjectGetDataPtr(o);
  fputc('(', stdout);
  while (l->value) {
    if (l->value->header.type->printFn && l->value->header.type->printFn->isBuiltIn) {
      StackPush(ctx, l->value);
      l->value->header.type->printFn->builtIn(ctx);
      if (l->next && ListP(l->next) && ((List*)ObjectGetDataPtr(l->next))->value) {
        fputc(' ', stdout);
      }
    }
    if (!l->next) {
      break;
    }
    o = l->next;
    if (!ListP(o)) {
      ThrowUnexpectedType(ctx);
    }
    l = ObjectGetDataPtr(o);
  }
  fputc(')', stdout);
}

static int ListEmpty(List* lst) {
  return lst->next == NULL && lst->value == NULL;
}

static Object* ListFirst(List* lst) {
  return lst->value;
}

static Object* ListRest(List* lst) {
  return lst->next;
}

static void ListEval(Context* ctx) {
  Object* o = StackPop(ctx);
  if (!ListP(o)) {
    ThrowUnexpectedType(ctx);
  }
  List* l = ObjectGetDataPtr(o);
  if (ListEmpty(l)) {
    StackPush(ctx, o); // empty list evals to itself
    return;
  }
  Object* first = ListFirst(l);
  if (!first) {
    // TODO: Change? This is a weird case. Should use a Nothing type for nil and introduce variant types.
    // It is currently impossible to distinguish between "no value" and an intentional nil.
    ThrowError(ctx, ErrorNew(ctx, "Cannot apply nil"));
  }
  if (first->header.type->evalFn != NULL) {
    if (first->header.type->evalFn->isBuiltIn) {
      StackPush(ctx, first);
      first->header.type->evalFn->builtIn(ctx);
      first = StackPop(ctx);
    }
    else {
      abort(); // TODO: handle eval of non built in functions
    }
  }
  if (!first) {
    // TODO: Change? This is a weird case. Should use a Nothing type for nil and introduce variant types.
    // It is currently impossible to distinguish between "no value" and an intentional nil.
    ThrowError(ctx, ErrorNew(ctx, "Cannot apply nil"));
  }
  if (!first->header.type->applyFn) {
    size_t bufsize = strlen(first->header.type->name);
    bufsize += sizeof("Cannot apply  ");
    char* buf = malloc(bufsize);
    if (!buf) {
      ThrowOOM(ctx);
    }
    sprintf(buf, "Cannot apply %s", first->header.type->name);
    Object* o = ErrorNew(ctx, buf);
    free(buf);
    ThrowError(ctx, o);
  }
  StackPush(ctx, ListRest(l));
  StackPush(ctx, first);
  if (first->header.type->applyFn->isBuiltIn) {
    first->header.type->applyFn->builtIn(ctx);
  }
  else {
    abort(); // TODO: handle non built in functions
  }
}

static Function fListPrint;
static Function fListEval;

static int FunctionP(Object* o) {
  return o->header.type == &tFunction;
}

static void FunctionPrint(Context* ctx) {
  Object* o = StackPop(ctx);
  if (!FunctionP(o)) {
    ThrowUnexpectedType(ctx);
  }
  Function* f = ObjectGetDataPtr(o);
  printf("#<Function [%s]>", f->name);
}

static void FunctionApply(Context* ctx) {
  Object* o = StackPop(ctx);
  if (!FunctionP(o)) {
    ThrowUnexpectedType(ctx);
  }
  Function* f = ObjectGetDataPtr(o);

  // WIP
}

static Function fFunctionPrint;
static Function fFunctionApply;

static int ErrorP(Object* o) {
  return o->header.type == &tError;
}

static void ErrorDelete(Context* ctx) {
  Object* o = StackPop(ctx);
  if (!ErrorP(o)) {
    ThrowUnexpectedType(ctx);
  }
  Error* e = ObjectGetDataPtr(o);
  free(e->message);
  e->message = NULL;
}

static void ErrorPrint(Context* ctx) {
  Object* o = StackPop(ctx);
  if (!ErrorP(o)) {
    ThrowUnexpectedType(ctx);
  }
  Error* e = ObjectGetDataPtr(o);
  fputs(e->message, stdout);
}

static Function fErrorDelete;
static Function fErrorPrint;

static void initBuiltins() {
  // TODO: make thread safe
  static int initDone = 0;
  if (initDone) {
    return;
  }
  initDone = 1;

  // Error

  tError.alignment = sizeof(char*);
  tError.nFields = 0;
  tError.size = sizeof(Error);
  tError.fields = NULL;
  tError.name = "Error";
  tError.evalFn = NULL;
  tError.applyFn = NULL;

  fErrorDelete._header.marked = 1;
  fErrorDelete._header.next = NULL;
  fErrorDelete._header.type = &tFunction;
  fErrorDelete.name = "error-delete";
  fErrorDelete.isBuiltIn = 1;
  fErrorDelete.builtIn = &ErrorDelete;
  tError.deleteFn = &fErrorDelete;

  fErrorPrint._header.marked = 1;
  fErrorPrint._header.next = NULL;
  fErrorPrint._header.type = &tFunction;
  fErrorPrint.name = "error-print";
  fErrorPrint.isBuiltIn = 1;
  fErrorPrint.builtIn = &ErrorPrint;
  tError.printFn = &fErrorPrint;

  // Number

  tNumber.alignment = sizeof(double);
  tNumber.nFields = 0;
  tNumber.size = sizeof(Number);
  tNumber.fields = NULL;
  tNumber.name = "Number";

  tNumber.deleteFn = NULL;
  tNumber.evalFn = NULL;
  tNumber.applyFn = NULL;

  fNumberPrint._header.marked = 1;
  fNumberPrint._header.next = NULL;
  fNumberPrint._header.type = &tFunction;
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

  tSymbol.applyFn = NULL;

  fSymbolDelete._header.marked = 1;
  fSymbolDelete._header.next = NULL;
  fSymbolDelete._header.type = &tFunction;
  fSymbolDelete.name = "symbol-delete";
  fSymbolDelete.isBuiltIn = 1;
  fSymbolDelete.builtIn = &SymbolDelete;
  tSymbol.deleteFn = &fSymbolDelete;

  fSymbolPrint._header.marked = 1;
  fSymbolPrint._header.next = NULL;
  fSymbolPrint._header.type = &tFunction;
  fSymbolPrint.name = "symbol-print";
  fSymbolPrint.isBuiltIn = 1;
  fSymbolPrint.builtIn = &SymbolPrint;
  tSymbol.printFn = &fSymbolPrint;

  fSymbolEval._header.marked = 1;
  fSymbolEval._header.next = NULL;
  fSymbolEval._header.type = &tFunction;
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
  tList.applyFn = NULL;

  fListPrint._header.marked = 1;
  fListPrint._header.next = NULL;
  fListPrint._header.type = &tFunction;
  fListPrint.name = "list-print";
  fListPrint.isBuiltIn = 1;
  fListPrint.builtIn = &ListPrint;
  tList.printFn = &fListPrint;

  fListEval._header.marked = 1;
  fListEval._header.next = NULL;
  fListEval._header.type = &tFunction;
  fListEval.name = "list-eval";
  fListEval.isBuiltIn = 1;
  fListEval.builtIn = ListEval;
  tList.evalFn = &fListEval;

  // Function

  tFunction.alignment = sizeof(void*);
  tFunction.nFields = 0;
  tFunction.size = sizeof(Function);
  tFunction.fields = NULL;
  tFunction.name = "Function";

  tFunction.deleteFn = NULL;
  tFunction.evalFn = NULL;

  fFunctionPrint._header.marked = 1;
  fFunctionPrint._header.next = NULL;
  fFunctionPrint._header.type = &tFunction;
  fFunctionPrint.name = "function-print";
  fFunctionPrint.isBuiltIn = 1;
  fFunctionPrint.builtIn = &FunctionPrint;
  tFunction.printFn = &fFunctionPrint;

  // Static errors
  eOOM._header.marked = 1;
  eOOM._header.next = NULL;
  eOOM._header.type = &tError;
  eOOM.message = "Out of memory";
}

static Runtime* RuntimeNew(StreamType inputType, const char* strOrFileName) {
  Runtime* rt = (Runtime*)malloc(sizeof(Runtime));
  if (!rt) {
    goto cleanup;
  }

  initBuiltins();

  rt->nContexts = 1;
  rt->contextListSize = 100;
  rt->contexts = NULL;

  rt->environment = EnvironmentNew(NULL);
  if (!rt->environment) {
    goto cleanup;
  }

  rt->contexts = (Context**)malloc(sizeof(Context*) * rt->contextListSize);
  if (!rt->contexts) {
    goto cleanup;
  }

  for (unsigned int i = 0; i < rt->contextListSize; ++i) {
    rt->contexts[i] = NULL;
  }

  rt->currentContext = ContextNew(rt, inputType, strOrFileName);
  if (!rt->currentContext) {
    goto cleanup;
  }

  rt->contexts[0] = rt->currentContext;

  goto end;

cleanup:
  if (rt) {
    free(rt->contexts);
    EnvironmentDelete(rt->environment);
    free(rt);
    rt = NULL;
  }

end:
  return rt;
}

static void RuntimeDelete(Runtime* rt) {
  if (!rt) {
    return;
  }

  for (unsigned int i = 0; i < rt->contextListSize; ++i) {
    ContextDelete(rt->contexts[i]);
  }
  free(rt->contexts);

  EnvironmentDelete(rt->environment);
  free(rt);
}

static int StreamEnd(Stream* s) {
  if (s->type == ST_STRING) {
    return s->string[s->stringPos] == 0;
  }
  else if (s->type == ST_FILE) {
    return feof(s->file);
  }
  abort();
}

// Returns a value between 0 and 255, or -1 on end of input.
static int StreamGet(Stream* s) {
  if (StreamEnd(s)) {
    return -1;
  }
  if (s->type == ST_STRING) {
    return s->string[s->stringPos++];
  }
  else if (s->type == ST_FILE) {
    return fgetc(s->file);
  }
  abort();
}


// Returns the next token or NULL on end of input.
static void TokenizerNext(Context* ctx, Tokenizer* tokenizer, const char** token) {
  const char ws[] = " \n\r\t\v\b\f"; // 7
  const char delims[] = "()[]{}"; // 6

  if (StreamEnd(tokenizer->stream)) {
    (*token) = NULL;
    return;
  }

  unsigned int pos = 0;
  while (1) {
    if (tokenizer->c == -1) {
      if (pos == 0) {
        (*token) = NULL;
        return;
      }
      goto end;
    }

    for (unsigned int i = 0; i < 7; ++i) {
      if (tokenizer->c == ws[i]) {
        if (pos == 0) {
          goto nextChar;
        }
        else {
          tokenizer->c = StreamGet(tokenizer->stream);
          goto end;
        }
      }
    }

    for (unsigned int i = 0; i < 6; ++i) {
      if (tokenizer->c == delims[i]) {
        if (pos != 0) {
          goto end;
        }
        else {
          tokenizer->token[pos++] = tokenizer->c;
          tokenizer->c = StreamGet(tokenizer->stream);
          goto end;
        }
      }
    }

    if (pos == tokenizer->tokenSize) {
      unsigned int newSize = tokenizer->tokenSize * 2;
      char* newToken = (char*)realloc(tokenizer->token, newSize);
      if (newToken == NULL) {
        (*token) = NULL;
        ThrowOOM(ctx);
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
  (*token) = tokenizer->token;
  return;
}

static void ObjectMark(Object* o) {
  // WIP
  if (o->header.marked) {
    return;
  }
  o->header.marked = 1;
  Field* field;
  //o->header.type->fields // have to fix fields!
}

static void ContextCollectGarbage(Context* ctx) {
  // WIP
  // 1. Mark
  unsigned int slot = ctx->stack->top;
  while (slot) {
    ObjectMark(ctx->stack->data[slot--]);
  }
  // 2. Sweep
}

static Object* ObjectAllocRaw(Context* ctx, Type* type) {
  unsigned int allocSize = sizeof(Object) + sizeof(void*) + type->alignment - 1 + type->size;
  if (ctx->bytesAllocated + allocSize >= ctx->collectionThreshold) {
    ContextCollectGarbage(ctx);
    if (ctx->bytesAllocated + allocSize >= ctx->collectionThreshold) {
      if (allocSize > ctx->collectionThreshold) {
        ctx->collectionThreshold = allocSize * 2;
      }
      else {
        ctx->collectionThreshold *= 2;
      }
    }
  }
  Object* o = malloc(allocSize);
  if (!o) {
    ThrowOOM(ctx);
  }

  o->header.type = type;
  o->header.next = ctx->lastObject;
  o->header.marked = 0;

  ctx->bytesAllocated += allocSize;
  ctx->lastObject = o;

  return o;
}

static Object* ReaderReadInternal(Context* ctx, Reader* r);

static Object* readNumber(Context* ctx, const char* token) {
  Object* result;
  char* endptr;
  double number = strtod(token, &endptr);
  if (endptr > token) {
    result = ObjectAllocRaw(ctx, &tNumber);
    Number* n = ObjectGetDataPtr(result);
    n->value = number;
    return result;
  }
  return NULL;
}

static Object* readList(Context* ctx, const char* token, Reader* r) {
  if (strcmp(token, "(") != 0) {
    return NULL;
  }
  TokenizerNext(ctx, r->tokenizer, &token);
  if (!token) {
    return NULL;
  }
  Object* headObj = ObjectAllocRaw(ctx, &tList);
  List* lst = ObjectGetDataPtr(headObj);
  lst->value = NULL;
  lst->next = NULL;
  while (strcmp(token, ")")) {
    Object* value = ReaderReadInternal(ctx, r);
    if (!value) {
      ThrowError(ctx, ErrorNew(ctx, "Unexpected end of input"));
    }
    if (lst->value) {
      lst->next = ObjectAllocRaw(ctx, &tList);
      lst = ObjectGetDataPtr(lst->next);
      lst->value = NULL;
      lst->next = NULL;
    }
    lst->value = value;
    TokenizerNext(ctx, r->tokenizer, &token);
    if (!token) {
      ThrowError(ctx, ErrorNew(ctx, "Unexpected end of input"));
    }
  }
  return headObj;
}

static Object* SymbolNew(Context* ctx, const char* name) {
  Object* symObj = ObjectAllocRaw(ctx, &tSymbol);
  Symbol* sym = ObjectGetDataPtr(symObj);
  sym->name = strdup(name);
  if (!sym->name) {
    ThrowOOM(ctx);
  }
  return symObj;
}

static Object* ErrorNew(Context* ctx, const char* message) {
  Object* errObj = ObjectAllocRaw(ctx, &tError);
  Error* e = ObjectGetDataPtr(errObj);
  e->message = strdup(message);
  if (!e->message) {
    ThrowOOM(ctx);
  }
  return errObj;
}

static Object* readSymbol(Context* ctx, const char* token) {
  return SymbolNew(ctx, token);
}

static Object* ReaderReadInternal(Context* ctx, Reader* r) {
  const char* token = r->tokenizer->token;
  Object* result = readNumber(ctx, token);
  if (!result) {
    result = readList(ctx, token, r);
  }
  if (!result) {
    result = readSymbol(ctx, token);
  }

  return result;
}

// Returns NULL on end of input
static Object* ReaderRead(Context* ctx, Reader* r) {
  const char* token;
  TokenizerNext(ctx, r->tokenizer, &token);
  if (!token) {
    return NULL;
  }
  return ReaderReadInternal(ctx, r);
}

static Object* EnvironmentGet(Context* ctx, Symbol* name) {
  for (unsigned int i = 0; i < ctx->environment->bindingsListSize; ++i) {
    if (ctx->environment->names[i] && strcmp(ctx->environment->names[i], name->name) == 0) {
      return ctx->environment->objects[i];
    }
  }
  return NULL;
}

// Returns previous value, or NULL if none
static Object* EnvironmentBind(Context* ctx, Symbol* name, Object* obj) {
  unsigned int freeSlot = 0;
  char hasFree = 0;
  for (unsigned int i = 0; i < ctx->environment->bindingsListSize; ++i) {
    if (!hasFree && ctx->environment->names[i] == NULL) {
      hasFree = 1;
      freeSlot = i;
    }
    if (ctx->environment->names[i] &&
      strcmp(ctx->environment->names[i], name->name) == 0) {
      Object* previous = ctx->environment->objects[i];
      ctx->environment->objects[i] = obj;
      return previous;
    }
  }
  if (!hasFree) {
    unsigned int newBindingsSize = ctx->environment->bindingsListSize * 2;
    char** newNames = realloc(ctx->environment->names, newBindingsSize * sizeof(char*));
    if (!newNames) {
      ThrowOOM(ctx);
    }
    for (unsigned int i = ctx->environment->bindingsListSize; i < newBindingsSize; ++i) {
      newNames[i] = NULL;
    }
    Object** newObjects = realloc(ctx->environment->objects, newBindingsSize * sizeof(Object*));
    if (!newObjects) {
      ThrowOOM(ctx);
    }
    hasFree = 1;
    freeSlot = ctx->environment->bindingsListSize;
    ctx->environment->bindingsListSize = newBindingsSize;
    ctx->environment->names = newNames;
    ctx->environment->objects = newObjects;
  }
  ctx->environment->names[freeSlot] = strdup(name->name);
  if (!ctx->environment->names[freeSlot]) {
    ThrowOOM(ctx);
  }
  ctx->environment->objects[freeSlot] = obj;
  return NULL;
}

// Entry point

Function fTestUnwind;

void testUnwind(Context* ctx) {
  printf("UNWIND ACTION!\n");
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    fputs("Give program please.\n", stderr);
    return -1;
  }
  Runtime* rt = RuntimeNew(ST_FILE, argv[1]);
  if (!rt) {
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

  fTestUnwind._header.marked = 0;
  fTestUnwind._header.next = NULL;
  fTestUnwind._header.type = &tFunction;
  fTestUnwind.isBuiltIn = 1;
  fTestUnwind.builtIn = testUnwind;
  fTestUnwind.name = "TestUnwind";

  ContextPushUnwindAction(ctx, (Object*)&fTestUnwind);

jmppoint:;

  int jmpResult = setjmp(ctx->jmpBuf);
  if (jmpResult) {
    Object* o = ctx->error;
    if (!o) {
      fprintf(stderr, "Unknown error\n");
    }
    else {
      Error* e = ObjectGetDataPtr(o);
      fprintf(stderr, "Error: %s\n", e->message);
    }
    ClearError(ctx);
    goto jmppoint;
  }

  Object* o = ReaderRead(ctx, r);
  while (o) {
    if (o->header.type->evalFn && o->header.type->evalFn->isBuiltIn) {
      StackPush(ctx, o);
      o->header.type->evalFn->builtIn(ctx);
      o = StackPop(ctx);
    }
    if (!o) {
      puts("nil");
    }
    else if (o->header.type->printFn && o->header.type->printFn->isBuiltIn) {
      StackPush(ctx, o);
      o->header.type->printFn->builtIn(ctx);
      putc('\n', stdout);
    }
    o = ReaderRead(ctx, r);
  }

  RuntimeDelete(rt);
  return 0;
}
