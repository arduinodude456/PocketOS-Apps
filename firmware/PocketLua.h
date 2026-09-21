#ifndef POCKETLUA_H
#define POCKETLUA_H

// PocketLua: a bounded, allocation-free Lua-like runtime for Arduino/ESP32.
// The only required platform include is Arduino.h.
#include <Arduino.h>

#ifndef POCKETLUA_MAX_TOKENS
#define POCKETLUA_MAX_TOKENS 512
#endif
#ifndef POCKETLUA_MAX_VARS
#define POCKETLUA_MAX_VARS 48
#endif
#ifndef POCKETLUA_MAX_FUNCS
#define POCKETLUA_MAX_FUNCS 20
#endif
#ifndef POCKETLUA_MAX_EVENTS
#define POCKETLUA_MAX_EVENTS 12
#endif
#ifndef POCKETLUA_MAX_ARGS
#define POCKETLUA_MAX_ARGS 8
#endif
#ifndef POCKETLUA_NAME_SIZE
#define POCKETLUA_NAME_SIZE 32
#endif
#ifndef POCKETLUA_STRING_SIZE
#define POCKETLUA_STRING_SIZE 96
#endif
#ifndef POCKETLUA_MAX_STEPS
#define POCKETLUA_MAX_STEPS 10000UL
#endif
#ifndef POCKETLUA_MAX_TABLES
#define POCKETLUA_MAX_TABLES 32
#endif
#ifndef POCKETLUA_MAX_TABLE_ITEMS
#define POCKETLUA_MAX_TABLE_ITEMS 64
#endif
#ifndef POCKETLUA_MAX_TABLE_VALUES
#define POCKETLUA_MAX_TABLE_VALUES 256
#endif

/** Firmware-provided binding for the built-in pocketos.* functions.
 * Every callback is optional. `on` is a notification that a script registered
 * an event; the firmware dispatches that event with PocketLua::dispatch(). */
struct PocketLuaHost {
  void *context;
  void (*label)(void *context, int x, int y, int size, const char *text);
  void (*value)(void *context, int x, int y, int size, const char *text);
  void (*grid)(void *context, int x, int y, int columns, int rows, int cellSize);
  void (*cell)(void *context, int x, int y, int cellSize, int column, int row, const char *style);
  void (*button)(void *context, int x, int y, int width, int height, const char *text, const char *action);
  void (*message)(void *context, const char *text);
  void (*timer)(void *context, unsigned long milliseconds);
  long (*random)(void *context, long exclusiveMaximum);
  void (*on)(void *context, const char *eventName);
};

class PocketLua {
public:
  enum ValueType { NIL, BOOLEAN, NUMBER, STRING, FUNCTION, TABLE };
  struct Value {
    ValueType type;
    float number;
    bool boolean;
    int function;
    int table;
    char string[POCKETLUA_STRING_SIZE];
  };

  PocketLua() : _tokenCount(0), _varCount(0), _funcCount(0), _eventCount(0),
                _scope(0), _steps(0), _returning(false), _randomState(0x13579BDFUL) {
    clearHost();
    clearTables();
    clearError();
    _returnValue = nil();
  }

  void setHost(const PocketLuaHost &host) { _host = host; }
  const PocketLuaHost &host() const { return _host; }
  const char *error() const { return _error; }
  bool ok() const { return _error[0] == 0; }

  /** Tokenizes source and resets variables, functions, and event handlers. Source
   * must remain in memory until run()/dispatch() calls which use it have completed. */
  bool load(const char *source) {
    _tokenCount = _varCount = _funcCount = _eventCount = 0;
    _scope = 0; _steps = 0; _returning = false; clearError();
    _returnValue = nil(); clearTables();
    if (!source) { fail("null source"); return false; }
    const char *p = source;
    unsigned int line = 1;
    while (*p && ok()) {
      char c = *p;
      if (c == ' ' || c == '\t' || c == '\r') { ++p; continue; }
      if (c == '\n') { ++line; ++p; continue; }
      if (c == '-' && p[1] == '-') {
        p += 2;
        if (p[0] == '[' && p[1] == '[') {
          p += 2;
          while (*p && !(p[0] == ']' && p[1] == ']')) { if (*p == '\n') ++line; ++p; }
          if (*p) p += 2;
        } else { while (*p && *p != '\n') ++p; }
        continue;
      }
      if (isNameStart(c)) {
        const char *start = p++; while (isNamePart(*p)) ++p;
        addToken(keyword(start, (unsigned int)(p - start)), start, (unsigned int)(p - start), 0, line);
        continue;
      }
      if ((c >= '0' && c <= '9') || (c == '.' && p[1] >= '0' && p[1] <= '9')) {
        const char *start = p; bool dot = false;
        while ((*p >= '0' && *p <= '9') || (!dot && *p == '.')) { if (*p == '.') dot = true; ++p; }
        float value = 0.0f; const char *q = start;
        while (q < p && *q != '.') { value = value * 10.0f + (float)(*q++ - '0'); }
        if (q < p && *q == '.') { float scale = 0.1f; ++q; while (q < p) { value += (float)(*q++ - '0') * scale; scale *= 0.1f; } }
        addToken(T_NUMBER, start, (unsigned int)(p - start), value, line); continue;
      }
      if (c == '\'' || c == '\"') {
        char quote = c; const char *start = ++p;
        while (*p && *p != quote) { if (*p == '\\' && p[1]) ++p; if (*p == '\n') ++line; ++p; }
        if (*p != quote) { fail("unterminated string"); break; }
        addToken(T_STRING, start, (unsigned int)(p - start), 0, line); ++p; continue;
      }
      TokenType type = T_EOF; unsigned int length = 1;
      if (c == '=' && p[1] == '=') { type = T_EQ; length = 2; }
      else if (c == '~' && p[1] == '=') { type = T_NE; length = 2; }
      else if (c == '<' && p[1] == '=') { type = T_LE; length = 2; }
      else if (c == '>' && p[1] == '=') { type = T_GE; length = 2; }
      else if (c == '.' && p[1] == '.') { type = T_CONCAT; length = 2; }
      else if (c == '+') type = T_PLUS; else if (c == '-') type = T_MINUS;
      else if (c == '*') type = T_STAR; else if (c == '/') type = T_SLASH;
      else if (c == '%') type = T_PERCENT; else if (c == '=') type = T_ASSIGN;
      else if (c == '<') type = T_LT; else if (c == '>') type = T_GT;
      else if (c == '(') type = T_LPAREN; else if (c == ')') type = T_RPAREN;
      else if (c == '{') type = T_LBRACE; else if (c == '}') type = T_RBRACE;
      else if (c == '[') type = T_LBRACKET; else if (c == ']') type = T_RBRACKET;
      else if (c == '#') type = T_HASH;
      else if (c == ',') type = T_COMMA; else if (c == ';') type = T_SEMI;
      else if (c == '.') type = T_DOT;
      else { fail("invalid character"); break; }
      addToken(type, p, length, 0, line); p += length;
    }
    if (ok()) addToken(T_EOF, p, 0, 0, line);
    return ok();
  }

  /** Executes the currently loaded chunk. */
  bool run() {
    if (!ok() || _tokenCount == 0) return false;
    _steps = 0; _returning = false; int pc = 0;
    executeBlock(pc, _tokenCount - 1);
    return ok();
  }

  /** Calls a handler registered through pocketos.on(event, function). */
  bool dispatch(const char *eventName, const char *argument = 0) {
    if (!eventName) return false;
    for (int i = 0; i < _eventCount; ++i) {
      if (sameText(_events[i].name, eventName)) {
        Value args[1]; int argc = 0;
        if (argument) { args[0] = stringValue(argument); argc = 1; }
        Value ignored = callFunction(_events[i].function, args, argc);
        (void)ignored;
        return ok();
      }
    }
    return false;
  }

  Value get(const char *name) const {
    int index = findVar(name); return index >= 0 ? _vars[index].value : nil();
  }

  static Value nil() { Value v; v.type = NIL; v.number = 0; v.boolean = false; v.function = -1; v.table = -1; v.string[0] = 0; return v; }
  static Value numberValue(float n) { Value v = nil(); v.type = NUMBER; v.number = n; return v; }
  static Value boolValue(bool b) { Value v = nil(); v.type = BOOLEAN; v.boolean = b; return v; }

private:
  enum TokenType {
    T_EOF, T_IDENT, T_NUMBER, T_STRING, T_TRUE, T_FALSE, T_NIL,
    T_LOCAL, T_IF, T_THEN, T_ELSE, T_ELSEIF, T_END, T_WHILE, T_FOR, T_DO,
    T_FUNCTION, T_RETURN, T_AND, T_OR, T_NOT,
    T_PLUS, T_MINUS, T_STAR, T_SLASH, T_PERCENT, T_CONCAT,
    T_EQ, T_NE, T_LT, T_LE, T_GT, T_GE, T_ASSIGN,
    T_LPAREN, T_RPAREN, T_LBRACE, T_RBRACE, T_LBRACKET, T_RBRACKET,
    T_COMMA, T_SEMI, T_DOT, T_HASH
  };
  struct Token { TokenType type; const char *start; unsigned int length; float number; unsigned int line; };
  struct Variable { char name[POCKETLUA_NAME_SIZE]; Value value; int scope; };
  struct Function { bool used; int bodyBegin; int bodyEnd; int parameterCount; char parameters[POCKETLUA_MAX_ARGS][POCKETLUA_NAME_SIZE]; };
  struct Event { char name[POCKETLUA_NAME_SIZE]; int function; };
  struct Table { bool used; bool marked; int length; int cells[POCKETLUA_MAX_TABLE_ITEMS]; };
  struct TableCell { bool used; Value value; };

  PocketLuaHost _host;
  Token _tokens[POCKETLUA_MAX_TOKENS]; int _tokenCount;
  Variable _vars[POCKETLUA_MAX_VARS]; int _varCount;
  Function _funcs[POCKETLUA_MAX_FUNCS]; int _funcCount;
  Event _events[POCKETLUA_MAX_EVENTS]; int _eventCount;
  Table _tables[POCKETLUA_MAX_TABLES];
  TableCell _tableCells[POCKETLUA_MAX_TABLE_VALUES];
  int _tempTables[POCKETLUA_MAX_TABLES]; int _tempTableCount;
  int _scope; unsigned long _steps; bool _returning; Value _returnValue;
  unsigned long _randomState; char _error[80];

  void clearHost() { _host.context = 0; _host.label = 0; _host.value = 0; _host.grid = 0; _host.cell = 0; _host.button = 0; _host.message = 0; _host.timer = 0; _host.random = 0; _host.on = 0; }
  void clearTables() {
    _tempTableCount = 0;
    for (int i = 0; i < POCKETLUA_MAX_TABLES; ++i) {
      _tables[i].used = false; _tables[i].marked = false; _tables[i].length = 0;
      for (int j = 0; j < POCKETLUA_MAX_TABLE_ITEMS; ++j) _tables[i].cells[j] = -1;
    }
    for (int i = 0; i < POCKETLUA_MAX_TABLE_VALUES; ++i) { _tableCells[i].used = false; _tableCells[i].value = nil(); }
  }
  void clearError() { _error[0] = 0; }
  void fail(const char *message) { if (_error[0]) return; unsigned int i = 0; while (message[i] && i + 1 < sizeof(_error)) { _error[i] = message[i]; ++i; } _error[i] = 0; }
  static bool isNameStart(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
  static bool isNamePart(char c) { return isNameStart(c) || (c >= '0' && c <= '9'); }
  static bool sameN(const char *a, const char *b, unsigned int n) { for (unsigned int i = 0; i < n; ++i) if (a[i] != b[i]) return false; return true; }
  static bool sameText(const char *a, const char *b) { unsigned int i = 0; while (a[i] || b[i]) { if (a[i] != b[i]) return false; ++i; } return true; }
  static void copyN(char *out, unsigned int capacity, const char *src, unsigned int n) { if (!capacity) return; unsigned int i = 0; while (i + 1 < capacity && i < n) { out[i] = src[i]; ++i; } out[i] = 0; }
  static void appendN(char *out, unsigned int capacity, const char *src, unsigned int n) { unsigned int at = 0; while (at < capacity && out[at]) ++at; unsigned int i = 0; while (at + 1 < capacity && i < n) out[at++] = src[i++]; if (capacity) out[at < capacity ? at : capacity - 1] = 0; }
  bool tokenIs(int p, TokenType type) const { return p >= 0 && p < _tokenCount && _tokens[p].type == type; }

  TokenType keyword(const char *s, unsigned int n) const {
    if (n == 4 && sameN(s, "true", n)) return T_TRUE;
    if (n == 5 && sameN(s, "false", n)) return T_FALSE;
    if (n == 3 && sameN(s, "nil", n)) return T_NIL;
    if (n == 5 && sameN(s, "local", n)) return T_LOCAL;
    if (n == 2 && sameN(s, "if", n)) return T_IF;
    if (n == 4 && sameN(s, "then", n)) return T_THEN;
    if (n == 4 && sameN(s, "else", n)) return T_ELSE;
    if (n == 6 && sameN(s, "elseif", n)) return T_ELSEIF;
    if (n == 3 && sameN(s, "end", n)) return T_END;
    if (n == 5 && sameN(s, "while", n)) return T_WHILE;
    if (n == 3 && sameN(s, "for", n)) return T_FOR;
    if (n == 2 && sameN(s, "do", n)) return T_DO;
    if (n == 8 && sameN(s, "function", n)) return T_FUNCTION;
    if (n == 6 && sameN(s, "return", n)) return T_RETURN;
    if (n == 3 && sameN(s, "and", n)) return T_AND;
    if (n == 2 && sameN(s, "or", n)) return T_OR;
    if (n == 3 && sameN(s, "not", n)) return T_NOT;
    return T_IDENT;
  }
  void addToken(TokenType type, const char *start, unsigned int length, float number, unsigned int line) {
    if (_tokenCount >= POCKETLUA_MAX_TOKENS) { fail("too many tokens"); return; }
    Token &t = _tokens[_tokenCount++]; t.type = type; t.start = start; t.length = length; t.number = number; t.line = line;
  }
  bool match(int &pc, TokenType type) { if (tokenIs(pc, type)) { ++pc; return true; } return false; }
  bool need(int &pc, TokenType type, const char *message) { if (match(pc, type)) return true; fail(message); return false; }
  void tokenName(const Token &token, char *out) const { copyN(out, POCKETLUA_NAME_SIZE, token.start, token.length); }

  void protectTable(const Value &value) {
    if (value.type != TABLE || value.table < 0 || value.table >= POCKETLUA_MAX_TABLES) return;
    for (int i = 0; i < _tempTableCount; ++i) if (_tempTables[i] == value.table) return;
    if (_tempTableCount < POCKETLUA_MAX_TABLES) _tempTables[_tempTableCount++] = value.table;
  }
  void markValue(const Value &value) { if (value.type == TABLE) markTable(value.table); }
  void markTable(int id) {
    if (id < 0 || id >= POCKETLUA_MAX_TABLES || !_tables[id].used || _tables[id].marked) return;
    Table &table = _tables[id]; table.marked = true;
    for (int i = 0; i < table.length; ++i) {
      int cell = table.cells[i];
      if (cell >= 0 && cell < POCKETLUA_MAX_TABLE_VALUES && _tableCells[cell].used) markValue(_tableCells[cell].value);
    }
  }
  void collectTables() {
    for (int i = 0; i < POCKETLUA_MAX_TABLES; ++i) _tables[i].marked = false;
    for (int i = 0; i < _varCount; ++i) markValue(_vars[i].value);
    markValue(_returnValue);
    for (int i = 0; i < _tempTableCount; ++i) markTable(_tempTables[i]);
    for (int i = 0; i < POCKETLUA_MAX_TABLES; ++i) {
      if (!_tables[i].used || _tables[i].marked) continue;
      for (int j = 0; j < POCKETLUA_MAX_TABLE_ITEMS; ++j) {
        int cell = _tables[i].cells[j];
        if (cell >= 0 && cell < POCKETLUA_MAX_TABLE_VALUES) { _tableCells[cell].used = false; _tableCells[cell].value = nil(); }
        _tables[i].cells[j] = -1;
      }
      _tables[i].used = false; _tables[i].length = 0;
    }
  }
  int allocateCell() {
    for (int i = 0; i < POCKETLUA_MAX_TABLE_VALUES; ++i) if (!_tableCells[i].used) { _tableCells[i].used = true; _tableCells[i].value = nil(); return i; }
    collectTables();
    for (int i = 0; i < POCKETLUA_MAX_TABLE_VALUES; ++i) if (!_tableCells[i].used) { _tableCells[i].used = true; _tableCells[i].value = nil(); return i; }
    fail("table value limit"); return -1;
  }
  Value newTable() {
    int id = -1;
    for (int i = 0; i < POCKETLUA_MAX_TABLES; ++i) if (!_tables[i].used) { id = i; break; }
    if (id < 0) {
      collectTables();
      for (int i = 0; i < POCKETLUA_MAX_TABLES; ++i) if (!_tables[i].used) { id = i; break; }
    }
    if (id < 0) { fail("table limit"); return nil(); }
    Table &table = _tables[id]; table.used = true; table.marked = false; table.length = 0;
    for (int i = 0; i < POCKETLUA_MAX_TABLE_ITEMS; ++i) table.cells[i] = -1;
    Value value = nil(); value.type = TABLE; value.table = id; protectTable(value); return value;
  }

  Value stringToken(const Token &token) const {
    Value v = nil(); v.type = STRING; unsigned int out = 0;
    for (unsigned int i = 0; i < token.length && out + 1 < POCKETLUA_STRING_SIZE; ++i) {
      char c = token.start[i];
      if (c == '\\' && i + 1 < token.length) { c = token.start[++i]; if (c == 'n') c = '\n'; else if (c == 't') c = '\t'; }
      v.string[out++] = c;
    }
    v.string[out] = 0; return v;
  }
  Value stringValue(const char *s) const { Value v = nil(); v.type = STRING; copyN(v.string, POCKETLUA_STRING_SIZE, s ? s : "", s ? textLength(s) : 0); return v; }
  static unsigned int textLength(const char *s) { unsigned int n = 0; while (s[n]) ++n; return n; }
  static bool truthy(const Value &v) { return v.type != NIL && !(v.type == BOOLEAN && !v.boolean); }
  static long asLong(const Value &v) { return v.type == NUMBER ? (long)v.number : 0; }
  const char *asText(const Value &v, char *scratch) const {
    if (v.type == STRING) return v.string;
    if (v.type == BOOLEAN) return v.boolean ? "true" : "false";
    if (v.type == NIL) return "nil";
    if (v.type == FUNCTION) return "function";
    if (v.type == TABLE) return "table";
    long n = (long)v.number; bool negative = n < 0; unsigned long u = (unsigned long)(negative ? -n : n);
    char digits[16]; unsigned int count = 0; do { digits[count++] = (char)('0' + u % 10); u /= 10; } while (u && count < sizeof(digits));
    unsigned int at = 0; if (negative) scratch[at++] = '-'; while (count) scratch[at++] = digits[--count]; scratch[at] = 0; return scratch;
  }

  int findVar(const char *name) const { for (int i = _varCount - 1; i >= 0; --i) if (sameText(_vars[i].name, name)) return i; return -1; }
  void setVar(const char *name, const Value &value, bool local) {
    int index = local ? -1 : findVar(name);
    if (index >= 0) { _vars[index].value = value; return; }
    if (_varCount >= POCKETLUA_MAX_VARS) { fail("variable limit"); return; }
    copyN(_vars[_varCount].name, POCKETLUA_NAME_SIZE, name, textLength(name)); _vars[_varCount].value = value; _vars[_varCount].scope = local ? _scope : 0; ++_varCount;
  }
  Value getVar(const char *name) const { int index = findVar(name); return index >= 0 ? _vars[index].value : nil(); }
  bool validTable(const Value &value) const { return value.type == TABLE && value.table >= 0 && value.table < POCKETLUA_MAX_TABLES && _tables[value.table].used; }
  int checkedIndex(const Value &value, int maximum, const char *message) {
    if (value.type != NUMBER) { fail("table index needs number"); return -1; }
    long index = (long)value.number;
    if (index < 1 || index > maximum || (float)index != value.number) { fail(message); return -1; }
    return (int)index;
  }
  Value tableGet(const Value &value, const Value &indexValue) {
    if (!validTable(value)) { fail("index needs table"); return nil(); }
    int index = checkedIndex(indexValue, POCKETLUA_MAX_TABLE_ITEMS, "table index out of range");
    if (index < 0 || index > _tables[value.table].length) return nil();
    int cell = _tables[value.table].cells[index - 1];
    if (cell < 0 || cell >= POCKETLUA_MAX_TABLE_VALUES || !_tableCells[cell].used) return nil();
    Value result = _tableCells[cell].value; protectTable(result); return result;
  }
  void tableSet(const Value &tableValue, const Value &indexValue, const Value &value) {
    if (!validTable(tableValue)) { fail("index assignment needs table"); return; }
    int index = checkedIndex(indexValue, POCKETLUA_MAX_TABLE_ITEMS, "table index out of range"); if (index < 0) return;
    Table &table = _tables[tableValue.table]; int &cell = table.cells[index - 1];
    if (value.type == NIL) {
      if (cell >= 0) { _tableCells[cell].used = false; _tableCells[cell].value = nil(); cell = -1; }
      return;
    }
    protectTable(value);
    if (cell < 0) { cell = allocateCell(); if (cell < 0) return; }
    _tableCells[cell].value = value;
    if (index > table.length) table.length = index;
  }
  int tableLength(const Value &value) {
    if (!validTable(value)) { fail("length needs table"); return 0; }
    Table &table = _tables[value.table];
    int length = table.length;
    while (length > 0 && table.cells[length - 1] < 0) --length;
    return length;
  }
  void tableInsert(const Value &tableValue, int position, const Value &value) {
    if (!validTable(tableValue)) { fail("table.insert needs table"); return; }
    Table &table = _tables[tableValue.table];
    int length = tableLength(tableValue); if (!ok()) return;
    table.length = length;
    if (length >= POCKETLUA_MAX_TABLE_ITEMS) { fail("table item limit"); return; }
    if (position < 1 || position > length + 1) { fail("insert position out of range"); return; }
    protectTable(tableValue); protectTable(value);
    int cell = allocateCell(); if (cell < 0) return;
    for (int i = length; i >= position; --i) table.cells[i] = table.cells[i - 1];
    table.cells[position - 1] = cell; _tableCells[cell].value = value; table.length = length + 1;
  }
  Value tableRemove(const Value &tableValue, int position) {
    if (!validTable(tableValue)) { fail("table.remove needs table"); return nil(); }
    Table &table = _tables[tableValue.table];
    int length = tableLength(tableValue); if (!ok() || length == 0) return nil();
    if (position < 1 || position > length) { fail("remove position out of range"); return nil(); }
    int removedCell = table.cells[position - 1]; Value result = nil();
    if (removedCell >= 0 && removedCell < POCKETLUA_MAX_TABLE_VALUES && _tableCells[removedCell].used) result = _tableCells[removedCell].value;
    for (int i = position; i < length; ++i) table.cells[i - 1] = table.cells[i];
    table.cells[length - 1] = -1; table.length = length - 1;
    if (removedCell >= 0) { _tableCells[removedCell].used = false; _tableCells[removedCell].value = nil(); }
    protectTable(result); return result;
  }
  void popScope(int scope) {
    // Globals may have been appended while a function was running, so this is
    // deliberately a compacting removal instead of a simple tail pop.
    int write = 0;
    for (int read = 0; read < _varCount; ++read) {
      if (_vars[read].scope == scope) continue;
      if (write != read) _vars[write] = _vars[read];
      ++write;
    }
    _varCount = write;
  }

  void executeBlock(int &pc, int stop) {
    while (ok() && !_returning && pc < stop && !tokenIs(pc, T_ELSE) && !tokenIs(pc, T_ELSEIF) && !tokenIs(pc, T_END)) {
      if (++_steps > POCKETLUA_MAX_STEPS) { fail("execution step limit"); return; }
      int tempBase = _tempTableCount;
      executeStatement(pc);
      _tempTableCount = tempBase;
      while (match(pc, T_SEMI)) {}
    }
  }
  void executeStatement(int &pc) {
    if (match(pc, T_LOCAL)) { executeLocal(pc); return; }
    if (match(pc, T_IF)) { executeIf(pc); return; }
    if (match(pc, T_WHILE)) { executeWhile(pc); return; }
    if (match(pc, T_FOR)) { executeFor(pc); return; }
    if (match(pc, T_RETURN)) { _returnValue = (tokenIs(pc, T_END) || tokenIs(pc, T_ELSE) || tokenIs(pc, T_ELSEIF) || tokenIs(pc, T_EOF) || tokenIs(pc, T_SEMI)) ? nil() : parseExpression(pc); _returning = true; return; }
    if (match(pc, T_FUNCTION)) { executeNamedFunction(pc, false); return; }
    if (tokenIs(pc, T_IDENT) && (tokenIs(pc + 1, T_ASSIGN) || tokenIs(pc + 1, T_COMMA))) { executeAssignment(pc, false); return; }
    if (tokenIs(pc, T_IDENT) && tokenIs(pc + 1, T_LBRACKET)) { executeIndexedAssignment(pc); return; }
    bool callStatement = startsCall(pc);
    Value v = parseExpression(pc);
    if (ok() && !callStatement && v.type != NIL && v.type != FUNCTION) fail("statement must be call");
  }
  bool startsCall(int pc) const {
    if (!tokenIs(pc, T_IDENT)) return false;
    ++pc;
    while (tokenIs(pc, T_DOT) && tokenIs(pc + 1, T_IDENT)) pc += 2;
    return tokenIs(pc, T_LPAREN);
  }
  void executeLocal(int &pc) {
    if (match(pc, T_FUNCTION)) { executeNamedFunction(pc, true); return; }
    executeAssignment(pc, true);
  }
  void executeAssignment(int &pc, bool local) {
    char names[POCKETLUA_MAX_ARGS][POCKETLUA_NAME_SIZE]; int count = 0;
    do {
      if (!tokenIs(pc, T_IDENT) || count >= POCKETLUA_MAX_ARGS) { fail("expected variable"); return; }
      tokenName(_tokens[pc++], names[count++]);
    } while (match(pc, T_COMMA));
    Value values[POCKETLUA_MAX_ARGS]; for (int i = 0; i < count; ++i) values[i] = nil();
    if (match(pc, T_ASSIGN)) {
      int i = 0; do { if (i >= count) { fail("too many values"); return; } values[i++] = parseExpression(pc); } while (match(pc, T_COMMA));
    }
    for (int i = 0; i < count; ++i) setVar(names[i], values[i], local);
  }
  void executeIndexedAssignment(int &pc) {
    char name[POCKETLUA_NAME_SIZE]; tokenName(_tokens[pc++], name); Value table = getVar(name);
    Value indexes[POCKETLUA_MAX_ARGS]; int count = 0;
    while (match(pc, T_LBRACKET)) {
      if (count >= POCKETLUA_MAX_ARGS) { fail("too many indexes"); return; }
      indexes[count++] = parseExpression(pc); if (!need(pc, T_RBRACKET, "expected ]")) return;
    }
    if (!count || !need(pc, T_ASSIGN, "expected =")) return;
    for (int i = 0; i + 1 < count; ++i) { table = tableGet(table, indexes[i]); if (!ok()) return; }
    Value value = parseExpression(pc); if (ok()) tableSet(table, indexes[count - 1], value);
  }
  void executeNamedFunction(int &pc, bool local) {
    if (!tokenIs(pc, T_IDENT)) { fail("expected function name"); return; }
    char name[POCKETLUA_NAME_SIZE]; tokenName(_tokens[pc++], name); Value f = makeFunction(pc); if (ok()) setVar(name, f, local);
  }

  void executeIf(int &pc) {
    Value condition = parseExpression(pc); if (!need(pc, T_THEN, "expected then")) return;
    if (truthy(condition)) { executeBlock(pc, _tokenCount); skipIfRemainder(pc); return; }
    seekIfMarker(pc);
    while (tokenIs(pc, T_ELSEIF) && ok()) {
      ++pc; condition = parseExpression(pc); if (!need(pc, T_THEN, "expected then")) return;
      if (truthy(condition)) { executeBlock(pc, _tokenCount); skipIfRemainder(pc); return; }
      seekIfMarker(pc);
    }
    if (match(pc, T_ELSE)) { executeBlock(pc, _tokenCount); need(pc, T_END, "expected end"); return; }
    need(pc, T_END, "expected end");
  }
  void seekIfMarker(int &pc) {
    int depth = 0;
    while (pc < _tokenCount) {
      TokenType t = _tokens[pc].type;
      if (t == T_IF || t == T_WHILE || t == T_FOR || t == T_FUNCTION) ++depth;
      else if (t == T_END) { if (!depth) return; --depth; }
      else if (!depth && (t == T_ELSE || t == T_ELSEIF)) return;
      ++pc;
    }
    fail("expected end");
  }
  void skipIfRemainder(int &pc) {
    if (tokenIs(pc, T_END)) { ++pc; return; }
    int depth = 0;
    while (pc < _tokenCount) {
      TokenType t = _tokens[pc++].type;
      if (t == T_IF || t == T_WHILE || t == T_FOR || t == T_FUNCTION) ++depth;
      else if (t == T_END) { if (!depth) return; --depth; }
    }
    fail("expected end");
  }
  int matchingEnd(int start) {
    int depth = 0;
    for (int p = start; p < _tokenCount; ++p) {
      TokenType t = _tokens[p].type;
      if (t == T_IF || t == T_WHILE || t == T_FOR || t == T_FUNCTION) ++depth;
      else if (t == T_END) { if (!depth) return p; --depth; }
    }
    fail("expected end"); return _tokenCount;
  }
  void executeWhile(int &pc) {
    int conditionStart = pc; Value test = parseExpression(pc); if (!need(pc, T_DO, "expected do")) return;
    int bodyStart = pc; int end = matchingEnd(bodyStart); if (!ok()) return;
    while (truthy(test) && ok() && !_returning) {
      int bodyPC = bodyStart; executeBlock(bodyPC, end);
      if (++_steps > POCKETLUA_MAX_STEPS) { fail("execution step limit"); return; }
      int testPC = conditionStart; test = parseExpression(testPC); if (!tokenIs(testPC, T_DO)) { fail("expected do"); return; }
    }
    pc = end + 1;
  }
  void executeFor(int &pc) {
    if (!tokenIs(pc, T_IDENT)) { fail("expected for variable"); return; }
    char name[POCKETLUA_NAME_SIZE]; tokenName(_tokens[pc++], name);
    if (!need(pc, T_ASSIGN, "expected =")) return;
    Value start = parseExpression(pc); if (!need(pc, T_COMMA, "expected ,")) return;
    Value finish = parseExpression(pc); Value step = numberValue(1);
    if (match(pc, T_COMMA)) step = parseExpression(pc);
    if (start.type != NUMBER || finish.type != NUMBER || step.type != NUMBER) { fail("for bounds need numbers"); return; }
    if (step.number == 0) { fail("for step must not be zero"); return; }
    if (!need(pc, T_DO, "expected do")) return;
    int bodyStart = pc; int end = matchingEnd(bodyStart); if (!ok()) return;
    int oldScope = _scope; ++_scope; int loopScope = _scope; setVar(name, start, true);
    float value = start.number;
    while (ok() && !_returning && (step.number > 0 ? value <= finish.number : value >= finish.number)) {
      int index = findVar(name); if (index < 0) { fail("for variable lost"); break; }
      _vars[index].value = numberValue(value);
      ++_scope; int iterationScope = _scope;
      int bodyPC = bodyStart; executeBlock(bodyPC, end);
      popScope(iterationScope); _scope = loopScope;
      if (++_steps > POCKETLUA_MAX_STEPS) { fail("execution step limit"); break; }
      value += step.number;
    }
    popScope(loopScope); _scope = oldScope; pc = end + 1;
  }

  Value parseExpression(int &pc) { return parseOr(pc); }
  Value parseOr(int &pc) { Value left = parseAnd(pc); while (match(pc, T_OR)) { Value right = parseAnd(pc); left = truthy(left) ? left : right; } return left; }
  Value parseAnd(int &pc) { Value left = parseCompare(pc); while (match(pc, T_AND)) { Value right = parseCompare(pc); left = truthy(left) ? right : left; } return left; }
  Value parseCompare(int &pc) {
    Value left = parseConcat(pc);
    while (tokenIs(pc, T_EQ) || tokenIs(pc, T_NE) || tokenIs(pc, T_LT) || tokenIs(pc, T_LE) || tokenIs(pc, T_GT) || tokenIs(pc, T_GE)) {
      TokenType op = _tokens[pc++].type; Value right = parseConcat(pc); bool r = false;
      if (op == T_EQ || op == T_NE) r = equal(left, right);
      else if (left.type == NUMBER && right.type == NUMBER) { if (op == T_LT) r = left.number < right.number; else if (op == T_LE) r = left.number <= right.number; else if (op == T_GT) r = left.number > right.number; else r = left.number >= right.number; }
      else { fail("comparison needs numbers"); return nil(); }
      left = boolValue(op == T_NE ? !r : r);
    }
    return left;
  }
  Value parseConcat(int &pc) {
    Value left = parseTerm(pc);
    while (match(pc, T_CONCAT)) {
      Value right = parseTerm(pc); char a[24], b[24];
      Value out = stringValue(asText(left, a));
      const char *rightText = asText(right, b);
      appendN(out.string, POCKETLUA_STRING_SIZE, rightText, textLength(rightText));
      left = out;
    }
    return left;
  }
  Value parseTerm(int &pc) {
    Value left = parseFactor(pc);
    while (tokenIs(pc, T_PLUS) || tokenIs(pc, T_MINUS)) { TokenType op = _tokens[pc++].type; Value right = parseFactor(pc); if (left.type != NUMBER || right.type != NUMBER) { fail("arithmetic needs numbers"); return nil(); } left = numberValue(op == T_PLUS ? left.number + right.number : left.number - right.number); }
    return left;
  }
  Value parseFactor(int &pc) {
    Value left = parseUnary(pc);
    while (tokenIs(pc, T_STAR) || tokenIs(pc, T_SLASH) || tokenIs(pc, T_PERCENT)) {
      TokenType op = _tokens[pc++].type; Value right = parseUnary(pc); if (left.type != NUMBER || right.type != NUMBER) { fail("arithmetic needs numbers"); return nil(); }
      if ((op == T_SLASH || op == T_PERCENT) && right.number == 0) { fail("division by zero"); return nil(); }
      if (op == T_STAR) left = numberValue(left.number * right.number); else if (op == T_SLASH) left = numberValue(left.number / right.number); else left = numberValue((float)((long)left.number % (long)right.number));
    }
    return left;
  }
  Value parseUnary(int &pc) {
    if (match(pc, T_NOT)) return boolValue(!truthy(parseUnary(pc)));
    if (match(pc, T_MINUS)) { Value v = parseUnary(pc); if (v.type != NUMBER) { fail("unary minus needs number"); return nil(); } return numberValue(-v.number); }
    if (match(pc, T_HASH)) { Value v = parseUnary(pc); return numberValue((float)tableLength(v)); }
    return parsePrimary(pc);
  }
  Value parsePrimary(int &pc) {
    Value value = nil();
    if (tokenIs(pc, T_NUMBER)) value = numberValue(_tokens[pc++].number);
    else if (tokenIs(pc, T_STRING)) value = stringToken(_tokens[pc++]);
    else if (match(pc, T_TRUE)) value = boolValue(true);
    else if (match(pc, T_FALSE)) value = boolValue(false);
    else if (match(pc, T_NIL)) value = nil();
    else if (match(pc, T_LPAREN)) { value = parseExpression(pc); need(pc, T_RPAREN, "expected )"); }
    else if (match(pc, T_FUNCTION)) value = makeFunction(pc);
    else if (match(pc, T_LBRACE)) {
      value = newTable(); if (!ok()) return nil();
      int index = 1;
      if (!tokenIs(pc, T_RBRACE)) {
        do {
          if (index > POCKETLUA_MAX_TABLE_ITEMS) { fail("table item limit"); return nil(); }
          Value item = parseExpression(pc); tableSet(value, numberValue((float)index++), item);
        } while (ok() && match(pc, T_COMMA) && !tokenIs(pc, T_RBRACE));
      }
      need(pc, T_RBRACE, "expected }");
    }
    else if (tokenIs(pc, T_IDENT)) {
      char name[POCKETLUA_NAME_SIZE]; tokenName(_tokens[pc++], name);
      while (match(pc, T_DOT)) { if (!tokenIs(pc, T_IDENT)) { fail("expected member name"); return nil(); } appendN(name, POCKETLUA_NAME_SIZE, ".", 1); appendN(name, POCKETLUA_NAME_SIZE, _tokens[pc].start, _tokens[pc].length); ++pc; }
      value = tokenIs(pc, T_LPAREN) ? callNamed(name, pc) : getVar(name);
    }
    else { fail("expected expression"); return nil(); }
    while (match(pc, T_LBRACKET)) {
      Value index = parseExpression(pc); if (!need(pc, T_RBRACKET, "expected ]")) return nil();
      value = tableGet(value, index); if (!ok()) return nil();
    }
    protectTable(value); return value;
  }
  static bool equal(const Value &a, const Value &b) { if (a.type != b.type) return false; if (a.type == NIL) return true; if (a.type == BOOLEAN) return a.boolean == b.boolean; if (a.type == NUMBER) return a.number == b.number; if (a.type == FUNCTION) return a.function == b.function; if (a.type == TABLE) return a.table == b.table; return sameText(a.string, b.string); }

  Value makeFunction(int &pc) {
    if (_funcCount >= POCKETLUA_MAX_FUNCS) { fail("function limit"); return nil(); }
    if (!need(pc, T_LPAREN, "expected (")) return nil();
    Function &f = _funcs[_funcCount]; f.used = true; f.parameterCount = 0;
    if (!tokenIs(pc, T_RPAREN)) {
      do { if (!tokenIs(pc, T_IDENT) || f.parameterCount >= POCKETLUA_MAX_ARGS) { fail("invalid parameter"); return nil(); } tokenName(_tokens[pc++], f.parameters[f.parameterCount++]); } while (match(pc, T_COMMA));
    }
    if (!need(pc, T_RPAREN, "expected )")) return nil();
    f.bodyBegin = pc; f.bodyEnd = matchingEnd(pc); if (!ok()) return nil();
    int id = _funcCount++; pc = f.bodyEnd + 1; Value v = nil(); v.type = FUNCTION; v.function = id; return v;
  }
  Value callNamed(const char *name, int &pc) {
    Value args[POCKETLUA_MAX_ARGS]; int argc = parseArguments(pc, args); if (!ok()) return nil();
    if (sameText(name, "table.insert")) {
      if (argc != 2 && argc != 3) { fail("table.insert needs table,[pos,]value"); return nil(); }
      if (!validTable(args[0])) { fail("table.insert needs table"); return nil(); }
      int position = tableLength(args[0]) + 1; Value item = args[1];
      if (argc == 3) { position = checkedIndex(args[1], POCKETLUA_MAX_TABLE_ITEMS, "insert position out of range"); item = args[2]; }
      if (ok()) tableInsert(args[0], position, item);
      return nil();
    }
    if (sameText(name, "table.remove")) {
      if (argc != 1 && argc != 2) { fail("table.remove needs table[,pos]"); return nil(); }
      if (!validTable(args[0])) { fail("table.remove needs table"); return nil(); }
      int position = tableLength(args[0]);
      if (argc == 2) position = checkedIndex(args[1], POCKETLUA_MAX_TABLE_ITEMS, "remove position out of range");
      return ok() ? tableRemove(args[0], position) : nil();
    }
    if (sameText(name, "pocketos.label")) { if (argc >= 4 && _host.label) { char s[24]; _host.label(_host.context, (int)asLong(args[0]), (int)asLong(args[1]), (int)asLong(args[2]), asText(args[3], s)); } return nil(); }
    if (sameText(name, "pocketos.value")) { if (argc >= 4 && _host.value) { char s[24]; _host.value(_host.context, (int)asLong(args[0]), (int)asLong(args[1]), (int)asLong(args[2]), asText(args[3], s)); } return nil(); }
    if (sameText(name, "pocketos.grid")) { if (argc >= 5 && _host.grid) _host.grid(_host.context, (int)asLong(args[0]), (int)asLong(args[1]), (int)asLong(args[2]), (int)asLong(args[3]), (int)asLong(args[4])); return nil(); }
    if (sameText(name, "pocketos.cell")) { if (argc >= 6 && _host.cell) { char s[24]; _host.cell(_host.context, (int)asLong(args[0]), (int)asLong(args[1]), (int)asLong(args[2]), (int)asLong(args[3]), (int)asLong(args[4]), asText(args[5], s)); } return nil(); }
    if (sameText(name, "pocketos.button")) { if (argc >= 6 && _host.button) { char text[24], action[24]; _host.button(_host.context, (int)asLong(args[0]), (int)asLong(args[1]), (int)asLong(args[2]), (int)asLong(args[3]), asText(args[4], text), asText(args[5], action)); } return nil(); }
    if (sameText(name, "pocketos.message")) { if (argc >= 1 && _host.message) { char s[24]; _host.message(_host.context, asText(args[0], s)); } return nil(); }
    if (sameText(name, "pocketos.timer")) { if (argc >= 1 && _host.timer) _host.timer(_host.context, (unsigned long)asLong(args[0])); return nil(); }
    if (sameText(name, "pocketos.random")) { long max = argc ? asLong(args[0]) : 0; if (max <= 0) return numberValue(0); if (_host.random) return numberValue((float)_host.random(_host.context, max)); _randomState = _randomState * 1103515245UL + 12345UL; return numberValue((float)((_randomState >> 16) % (unsigned long)max)); }
    if (sameText(name, "pocketos.on")) { if (argc >= 2 && args[0].type == STRING && args[1].type == FUNCTION) registerEvent(args[0].string, args[1].function); else fail("on needs string,function"); return nil(); }
    Value f = getVar(name); if (f.type == FUNCTION) return callFunction(f.function, args, argc); fail("unknown function"); return nil();
  }
  int parseArguments(int &pc, Value *args) {
    if (!need(pc, T_LPAREN, "expected (")) return 0;
    int count = 0;
    if (!tokenIs(pc, T_RPAREN)) { do { if (count >= POCKETLUA_MAX_ARGS) { fail("too many arguments"); return 0; } args[count++] = parseExpression(pc); } while (match(pc, T_COMMA)); }
    need(pc, T_RPAREN, "expected )"); return count;
  }
  Value callFunction(int id, Value *args, int argc) {
    if (id < 0 || id >= _funcCount || !_funcs[id].used) { fail("invalid function"); return nil(); }
    bool oldReturning = _returning; Value oldReturn = _returnValue; int oldScope = _scope; _returning = false; ++_scope; int functionScope = _scope;
    Function &f = _funcs[id]; for (int i = 0; i < f.parameterCount; ++i) setVar(f.parameters[i], i < argc ? args[i] : nil(), true);
    int pc = f.bodyBegin; executeBlock(pc, f.bodyEnd); Value result = _returning ? _returnValue : nil(); protectTable(result); popScope(functionScope); _scope = oldScope; _returning = oldReturning; _returnValue = oldReturn; return result;
  }
  void registerEvent(const char *name, int function) {
    for (int i = 0; i < _eventCount; ++i) if (sameText(_events[i].name, name)) { _events[i].function = function; if (_host.on) _host.on(_host.context, name); return; }
    if (_eventCount >= POCKETLUA_MAX_EVENTS) { fail("event limit"); return; }
    copyN(_events[_eventCount].name, POCKETLUA_NAME_SIZE, name, textLength(name)); _events[_eventCount++].function = function; if (_host.on) _host.on(_host.context, name);
  }
};

#endif  // POCKETLUA_H
