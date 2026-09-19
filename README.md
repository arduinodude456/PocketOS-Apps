# PocketOS Apps

SD apps for PocketOS. The `apps.txt` file is the store manifest.

PocketOS apps are declarative `.papp` source files interpreted by the generic SD App VM. Apps are not selected by a magic title and they do not call firmware functions such as `startSnake()` or `startCube()`. Use the generic variables, timers, events, buttons and drawing primitives described in [FORMAT.md](FORMAT.md).

The runtime keeps only a bounded active cache in RAM. App source and persistent variable state live on the SD card, so larger apps can carry more data without allocating a large global framebuffer or app object graph.
