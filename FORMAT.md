# PocketOS App Format

PocketOS apps are plain-text `.papp` files stored on the SD card. The firmware reads the source line by line; it does not compile app code and it does not identify apps by their title. Snake, 3D wireframes and other examples use the same generic VM primitives.

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

## Examples

`Snake.papp` demonstrates a timer-driven grid, SD-backed variables and a generic trail. `3DWuerfel.papp` demonstrates vertices, edges, rotation variables and a timer. These files do not call `startSnake()` or `startCube()` and their titles are not special to the firmware.

## Design limits

Keep hot variables small, use integer values where possible, and prefer timer events over tight loops. Apps should never access arbitrary SD paths; persistent state belongs in the PocketOS app-data namespace.
