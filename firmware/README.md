# PocketLua-Firmwarekern

`PocketLua.h` ist ein **header-only**, fremdcodefreier Lua-ähnlicher Interpreterkern für Arduino und ESP32. Er enthält außer `Arduino.h` keine Includes, verwendet keine dynamische Speicherallokation und ist für kleine, von SD geladene PocketOS-Apps gedacht. Die Größenlimits werden ausschließlich über Präprozessor-Makros eingestellt.

## Einbindung

```cpp
#include "PocketLua.h"

PocketLua vm;
PocketLuaHost host = {};
host.context = this;
host.label = [](void *ctx, int x, int y, int size, const char *text) {
  // UI des PocketOS zeichnen
};
// weitere gewünschte Callbacks setzen …
vm.setHost(host);

if (!vm.load(luaSource)) {
  Serial.println(vm.error());
} else if (!vm.run()) {
  Serial.println(vm.error());
}
```

Der Quelltext (`luaSource`) muss im Speicher bleiben, solange `run()` oder `dispatch()` verwendet werden, weil der Tokenizer Zeiger auf den Originaltext speichert. Nach `pocketos.on("touch", handler)` ruft die Firmware bei einem Ereignis `vm.dispatch("touch")` auf. Der Rückgabewert ist `false`, wenn kein Handler registriert ist oder ein Laufzeitfehler entstanden ist.

## Host-API

Alle Felder in `PocketLuaHost` sind optionale Funktionszeiger. Der erste Parameter jeder Callback-Funktion ist unverändert `host.context`. Nicht gesetzte Callbacks sind sichere No-ops. Das Feld `on` ist nur eine Benachrichtigung über eine Registrierung; es führt den Handler nicht selbst aus.

| Script-Aufruf | Callback-Signatur | Bedeutung |
|---|---|---|
| `pocketos.label(x, y, size, text)` | `label(ctx, int, int, int, const char*)` | Beschriftung zeichnen |
| `pocketos.value(x, y, size, text)` | `value(ctx, int, int, int, const char*)` | Wert/Status ausgeben |
| `pocketos.grid(x, y, cols, rows, cellSize)` | `grid(ctx, int, int, int, int, int)` | Raster anlegen/zeichnen |
| `pocketos.cell(x, y, cellSize, col, row, style)` | `cell(ctx, int, int, int, int, int, const char*)` | Rasterzelle zeichnen |
| `pocketos.button(x, y, w, h, text, action)` | `button(ctx, int, int, int, int, const char*, const char*)` | Touch-Schaltfläche zeichnen |
| `pocketos.message(text)` | `message(ctx, const char*)` | Meldung zeigen |
| `pocketos.timer(milliseconds)` | `timer(ctx, unsigned long)` | Tick-Intervall anfordern |
| `pocketos.random(max)` | `random(ctx, long)` | Ganzzahl im Bereich `0 .. max-1`; ohne Callback deterministischer Fallback |
| `pocketos.on(event, function)` | `on(ctx, const char*)` | Ereignishandler registrieren und Host benachrichtigen |

Die Hostseite ist für Zeichenfläche, Button-Hit-Testing, Timerplanung und den Aufruf von `dispatch()` verantwortlich. Beispielhafte Ereignisnamen sind `start`, `draw`, `tick` und `touch`.

## Unterstützte PocketLua-Syntax

Der Tokenizer erkennt Bezeichner, Dezimalzahlen, einfache und doppelte Strings mit `\\n`/`\\t`-Escapes, Zeilenkommentare (`-- ...`), Blockkommentare (`--[[ ... ]]`) und die unten genannten Operatoren. Whitespace und Semikolons sind optional.

```lua
-- Variablen, Mehrfachzuweisung und Ausdrücke
local x, y = 2, 3
local text = "Punkte: " .. (x + y)
x = x * 2 + y % 2

-- Vergleich und Logik
if x >= 4 and not false then
  pocketos.message(text)
elseif x == 3 then
  pocketos.message("drei")
else
  pocketos.message("anders")
end

-- Schleife
while x > 0 do
  x = x - 1
end

-- Begrenzte 1-basierte Arrays und numerische for-Schleifen
local snake = {{7, 5}, {6, 5}}
snake[1][1] = snake[1][1] + 1
table.insert(snake, {5, 5})
for i = 1, #snake do
  pocketos.message(snake[i][1])
end
local tail = table.remove(snake)       -- letztes Element
table.insert(snake, 1, tail)           -- optional mit Position

-- Benannte und anonyme Funktionen; Rückgabe und Argumente
function greet(name)
  return "Hallo " .. name
end
pocketos.on("start", function()
  pocketos.label(8, 8, 1, greet("PocketOS"))
end)
```

| Kategorie | Unterstützt |
|---|---|
| Werte | `nil`, `true`, `false`, Dezimalzahlen, Strings, Funktionswerte und begrenzte Array-Tabellen |
| Variablen | globale Variablen, `local`, Mehrfachzuweisung mit bis zu `POCKETLUA_MAX_ARGS` Namen/Werten |
| Tabellen/Arrays | Literale `{expr, ...}`, verschachtelte Tabellen, 1-basierter Zugriff `a[i]`, Zuweisung `a[i] = expr`, Länge `#a`, `table.insert(a [, pos], value)`, `table.remove(a [, pos])` |
| Operatoren | `+`, `-`, `*`, `/`, `%`, `..`, `#`, `==`, `~=`, `<`, `<=`, `>`, `>=`, `and`, `or`, `not`, Klammern und unäres `-` |
| Kontrollfluss | `if` / `elseif` / `else` / `end`, `while` / `do` / `end`, numerisches `for name = start, finish [, step] do ... end`, `return` |
| Funktionen | `function name(args) ... end`, `local function`, anonyme `function(args) ... end`, Aufrufe |
| Standardbibliothek | `pocketos.*` aus der obigen Tabelle sowie die Array-Builtins `table.insert` und `table.remove` |

## Feste Grenzen und Sicherheitsmodell

PocketLua ist ausdrücklich **kein vollständiges Lua 5.x** und führt weder Bytecode noch eingebettetes C/C++ aus. Tabellen sind bewusst auf den fortlaufenden, positiven Ganzzahlbereich `1 .. POCKETLUA_MAX_TABLE_ITEMS` beschränkt: Es gibt keine String-Schlüssel, Record-Felder, Iteratoren (`pairs`/`ipairs`), gemischten Tabellen oder Metatables. Die Länge `#a` entspricht dem höchsten fortlaufend belegten Array-Ende; Lücken innerhalb des Arrays sind möglich, aber wie in Lua sollte `#` dafür nicht als allgemeiner Belegungszähler verwendet werden. Eine Zuweisung von `nil` entfernt den betreffenden Eintrag. `table.insert` und `table.remove` verschieben Arrayelemente; ohne Position arbeiten sie am Ende.

Weiterhin nicht implementiert sind generische `for`-Schleifen, `repeat`, Closures mit eingefangenen lokalen Variablen, Methoden-Syntax (`:`), Punktnotation außer den Builtins `pocketos.*` und `table.*`, weitere Standardmodule (`string`, `math`), Dateizugriff, `require`, `goto` und Fehlerbehandlung mit `pcall`. Tabellen werden ausschließlich in festen internen Pools gehalten. Nicht mehr erreichbare Tabellenplätze können innerhalb dieser Pools wiederverwendet werden; es gibt weiterhin weder `malloc`/`new` noch einen dynamisch wachsenden Heap.

Arithmetik arbeitet mit `float`; `%` wandelt beide Operanden in `long`. Vergleiche `<`, `<=`, `>` und `>=` verlangen Zahlen. `==` und `~=` vergleichen Werte gleichen Typs. `and` und `or` liefern wie Lua einen der beiden Werte, werten ihre rechte Seite in dieser ersten Version jedoch **nicht kurzschlussartig** aus. Unbekannte Variablen liefern `nil`; unbekannte Funktionen und Syntax-/Laufzeitfehler setzen `vm.error()`.

Der Interpreter ist begrenzt durch `POCKETLUA_MAX_TOKENS` (512), `POCKETLUA_MAX_VARS` (48), `POCKETLUA_MAX_FUNCS` (20), `POCKETLUA_MAX_EVENTS` (12), `POCKETLUA_MAX_ARGS` (8), `POCKETLUA_NAME_SIZE` (32), `POCKETLUA_STRING_SIZE` (96), `POCKETLUA_MAX_TABLES` (32), `POCKETLUA_MAX_TABLE_ITEMS` (64 Elemente pro Tabelle), `POCKETLUA_MAX_TABLE_VALUES` (256 gleichzeitig belegte Arrayzellen über alle Tabellen) und `POCKETLUA_MAX_STEPS` (10.000). Die Makros können **vor** dem Include projektweit angepasst werden. Da die Pools als feste Felder in jeder `PocketLua`-Instanz liegen, erhöhen größere Werte unmittelbar deren statischen RAM-Bedarf. Das Schrittlimit begrenzt Endlosschleifen; für strikte Laufzeitbudgets sollte die Firmware zusätzlich eine App nur in kontrollierten Event-Zyklen dispatchen.
