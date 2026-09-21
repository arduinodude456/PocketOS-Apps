# PocketOS App Format

PocketOS apps are plain-text `.papp` or `.lua` files stored on the SD card. `.papp` files use the lightweight declarative VM; `.lua` files are executed by the embedded Lua 5.4 interpreter. Apps are discovered by extension and do not need firmware-specific app names.

## Memory model

App source files and persistent state are stored on the SD card. A bounded hot-variable cache is kept in RAM while an app is open. The current firmware accepts up to 32 active variables, 32 vertices, 48 edges and four trails. State is saved under `/PocketOS/AppData/<app-name>.papp.state` and is written when an app action or timer changes a value—not on every render call.

This is intentionally a hybrid design: reading every hot variable from SD for every pixel frame would be slow and would wear the card. Apps can still have a large source file and persistent data, while the active working set stays bounded and predictable.

## Syntax

```text
title=My App
accent=blue
var=counter,0
label=16,12,2,Hello PocketOS
button=16,220,120,32,Add,add,counter,1
timer=500,tick
event=tick,add,counter,1
event=tick,redraw
```

Supported primitives include `label`, `button`, `var`, `grid`, `cell`, `trail`, `vertex`, `edge`, `timer` and `event`. Button actions and event actions are generic commands: `set`, `add`, `toggle`, `wrap`, `random`, `trail`, `message`, `reset` and `redraw`. `reset` restores the declared `var` defaults and removes the persisted app state.

Conditional events use `event-if=event,var,operator,value,action`, where `operator` may be `==`, `!=`, `<`, `<=`, `>` or `>=`. The legacy two-coordinate equality form `event-if=event,xVar,xValue,yVar,yValue,action` is also supported for grid games.

The comma-separated button geometry ends after the fifth comma; everything after the label is the action. This allows actions such as `add,counter,1` without firmware-specific prefixes.

## Lua apps

Lua apps register lifecycle callbacks with `pocketos.on("start", fn)`, `pocketos.on("draw", fn)`, `pocketos.on("tick", fn)` and `pocketos.on("touch", fn)`. The display and input surface is deliberately small: `pocketos.label(x,y,size,text)`, `pocketos.value(x,y,size,text)`, `pocketos.grid(x,y,columns,rows,cell)`, `pocketos.cell(x,y,cell,gridX,gridY,kind)`, `pocketos.button(x,y,w,h,label,action)`, `pocketos.message(text)`, `pocketos.timer(milliseconds)` and `pocketos.random(maximum)`. Coordinates are relative to the content area, just like `.papp` files. Button actions are passed to the `touch` callback.

## Examples

`Snake.lua` demonstrates a real Lua app with a timer-driven grid, touch buttons, collision detection and food placement. `3DWuerfel.papp` demonstrates the declarative VM's vertices, edges, rotation variables and a timer. Apps do not call firmware-specific functions such as `startSnake()` or `startCube()`.

## Design limits

Keep hot variables small, use integer values where possible, and prefer timer events over tight loops. Apps should never access arbitrary SD paths; persistent state belongs in the PocketOS app-data namespace.
