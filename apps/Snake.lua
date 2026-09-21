-- title=Snake
-- accent=green
-- PocketLua-kompatible SD-App ohne Tabellen oder externe Module.

local cols, rows = 16, 10
local head_x, head_y = 7, 5
local dir_x, dir_y = 1, 0
local food_x, food_y = 11, 3
local score = 0
local alive = true

function reset()
  head_x, head_y = 7, 5
  dir_x, dir_y = 1, 0
  food_x, food_y = 11, 3
  score, alive = 0, true
  pocketos.message("")
end

function draw()
  pocketos.label(16, 4, 2, "SNAKE")
  if alive then
    pocketos.label(16, 29, 1, "Lenke die Schlange mit den Tasten")
  else
    pocketos.label(16, 29, 1, "Runde beendet - NEUES SPIEL")
  end
  pocketos.grid(104, 28, cols, rows, 7)
  pocketos.value(16, 100, 1, "Punkte: " .. score)
  pocketos.cell(104, 28, 7, head_x, head_y, "head")
  pocketos.cell(104, 28, 7, food_x, food_y, "food")
  pocketos.button(10, 126, 92, 28, "LINKS", "left")
  pocketos.button(114, 126, 92, 28, "HOCH", "up")
  pocketos.button(218, 126, 92, 28, "RECHTS", "right")
  pocketos.button(114, 158, 92, 28, "RUNTER", "down")
  pocketos.button(104, 190, 112, 20, "NEUES SPIEL", "reset")
end

function left()
  if dir_x ~= 1 then dir_x, dir_y = -1, 0 end
end

function right()
  if dir_x ~= -1 then dir_x, dir_y = 1, 0 end
end

function up()
  if dir_y ~= 1 then dir_x, dir_y = 0, -1 end
end

function down()
  if dir_y ~= -1 then dir_x, dir_y = 0, 1 end
end

function tick()
  if not alive then return end
  head_x, head_y = head_x + dir_x, head_y + dir_y
  if head_x < 0 or head_x >= cols or head_y < 0 or head_y >= rows then
    alive = false
    pocketos.message("Wand getroffen - verloren!")
  elseif head_x == food_x and head_y == food_y then
    score = score + 10
    food_x = pocketos.random(cols) - 1
    food_y = pocketos.random(rows) - 1
    pocketos.message("Punkt gesammelt!")
  end
end

pocketos.timer(260)
pocketos.on("start", function() end)
pocketos.on("draw", draw)
pocketos.on("tick", tick)
pocketos.on("left", left)
pocketos.on("right", right)
pocketos.on("up", up)
pocketos.on("down", down)
pocketos.on("reset", reset)
