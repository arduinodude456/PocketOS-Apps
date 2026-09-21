-- title=Snake
-- accent=green
-- PocketOS Lua API: pocketos.label, grid, cell, button, timer, on, message

local cols, rows = 16, 10
local snake = {{7, 5}, {6, 5}, {5, 5}}
local food = {11, 3}
local dx, dy = 1, 0
local score = 0
local alive = true

local function contains(x, y)
  for i = 1, #snake do
    if snake[i][1] == x and snake[i][2] == y then return true end
  end
  return false
end

local function place_food()
  repeat
    food[1] = pocketos.random(cols) - 1
    food[2] = pocketos.random(rows) - 1
  until not contains(food[1], food[2])
end

local function reset()
  snake = {{7, 5}, {6, 5}, {5, 5}}
  food = {11, 3}
  dx, dy, score, alive = 1, 0, 0, true
  pocketos.message("")
end

function draw()
  pocketos.label(16, 4, 2, "SNAKE")
  pocketos.label(16, 29, 1, alive and "Lenke die Schlange mit den Tasten" or "Runde beendet - NEUES SPIEL")
  pocketos.grid(104, 28, cols, rows, 7)
  pocketos.value(16, 100, 1, "Punkte: " .. score)
  for i = 1, #snake do
    pocketos.cell(104, 28, 7, snake[i][1], snake[i][2], i == 1 and "head" or "body")
  end
  pocketos.cell(104, 28, 7, food[1], food[2], "food")
  pocketos.button(10, 126, 92, 28, "LINKS", "left")
  pocketos.button(114, 126, 92, 28, "HOCH", "up")
  pocketos.button(218, 126, 92, 28, "RECHTS", "right")
  pocketos.button(114, 158, 92, 28, "RUNTER", "down")
  pocketos.button(104, 190, 112, 20, "NEUES SPIEL", "reset")
end

function touch(action)
  if action == "left" and dx ~= 1 then dx, dy = -1, 0
  elseif action == "right" and dx ~= -1 then dx, dy = 1, 0
  elseif action == "up" and dy ~= 1 then dx, dy = 0, -1
  elseif action == "down" and dy ~= -1 then dx, dy = 0, 1
  elseif action == "reset" then reset()
  end
end

function tick()
  if not alive then return end
  local head = snake[1]
  local nx, ny = head[1] + dx, head[2] + dy
  if nx < 0 or nx >= cols or ny < 0 or ny >= rows or contains(nx, ny) then
    alive = false
    pocketos.message("Wand oder eigener Körper getroffen!")
    return
  end
  table.insert(snake, 1, {nx, ny})
  if nx == food[1] and ny == food[2] then
    score = score + 10
    place_food()
    pocketos.message("Punkt gesammelt!")
  else
    table.remove(snake)
  end
end

pocketos.timer(260)
pocketos.on("draw", draw)
pocketos.on("tick", tick)
pocketos.on("touch", touch)
pocketos.on("start", function() draw() end)
