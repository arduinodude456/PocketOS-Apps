-- title=TicTacToe
-- accent=purple
local board = {0,0,0,0,0,0,0,0,0}
local moves, winner, game_over = 0, 0, false

function reset()
  board = {0,0,0,0,0,0,0,0,0}
  moves, winner, game_over = 0, 0, false
end
function win(a,b,c) return a ~= 0 and a == b and b == c end
function check()
  if win(board[1],board[2],board[3]) then winner=board[1] elseif win(board[4],board[5],board[6]) then winner=board[4]
  elseif win(board[7],board[8],board[9]) then winner=board[7] elseif win(board[1],board[4],board[7]) then winner=board[1]
  elseif win(board[2],board[5],board[8]) then winner=board[2] elseif win(board[3],board[6],board[9]) then winner=board[3]
  elseif win(board[1],board[5],board[9]) then winner=board[1] elseif win(board[3],board[5],board[7]) then winner=board[3] end
  if winner ~= 0 then game_over=true elseif moves >= 9 then game_over=true; winner=3 end
end
function computer()
  if game_over then return end
  if board[5] == 0 then board[5]=2 elseif board[1] == 0 then board[1]=2 elseif board[3] == 0 then board[3]=2
  elseif board[7] == 0 then board[7]=2 elseif board[9] == 0 then board[9]=2 elseif board[2] == 0 then board[2]=2
  elseif board[4] == 0 then board[4]=2 elseif board[6] == 0 then board[6]=2 elseif board[8] == 0 then board[8]=2 end
  moves=moves+1; check()
end
function mark(n)
  if game_over or board[n] ~= 0 then return end
  board[n]=1; moves=moves+1; check(); if not game_over then computer() end
end
function draw_mark(col,row,value)
  if value == 1 then pocketos.cell(42,54,46,col,row,"x") elseif value == 2 then pocketos.cell(42,54,46,col,row,"o") end
end
function draw()
  pocketos.label(16,4,2,"TIC-TAC-TOE")
  if game_over then
    if winner == 1 then pocketos.label(16,32,1,"Du gewinnst!") elseif winner == 2 then pocketos.label(16,32,1,"Computer gewinnt") else pocketos.label(16,32,1,"Unentschieden") end
  else pocketos.label(16,32,1,"Du bist X - Computer ist O") end
  pocketos.grid(42,54,3,3,46)
  draw_mark(0,0,board[1]); draw_mark(1,0,board[2]); draw_mark(2,0,board[3])
  draw_mark(0,1,board[4]); draw_mark(1,1,board[5]); draw_mark(2,1,board[6])
  draw_mark(0,2,board[7]); draw_mark(1,2,board[8]); draw_mark(2,2,board[9])
  pocketos.button(18,210,62,26,"FELD 1","field1"); pocketos.button(89,210,62,26,"FELD 2","field2"); pocketos.button(160,210,62,26,"FELD 3","field3")
  pocketos.button(18,238,62,26,"FELD 4","field4"); pocketos.button(89,238,62,26,"FELD 5","field5"); pocketos.button(160,238,62,26,"FELD 6","field6")
  pocketos.button(18,266,62,26,"FELD 7","field7"); pocketos.button(89,266,62,26,"FELD 8","field8"); pocketos.button(160,266,62,26,"FELD 9","field9")
  pocketos.button(89,296,100,20,"NEUES SPIEL","reset")
  pocketos.value(18,188,1,"Zuege: " .. moves)
end
function touch(action)
  if action == "reset" then reset() elseif action == "field1" then mark(1) elseif action == "field2" then mark(2) elseif action == "field3" then mark(3)
  elseif action == "field4" then mark(4) elseif action == "field5" then mark(5) elseif action == "field6" then mark(6)
  elseif action == "field7" then mark(7) elseif action == "field8" then mark(8) elseif action == "field9" then mark(9) end
end
pocketos.on("draw",draw)
pocketos.on("touch",touch)
