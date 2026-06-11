-- Equivalent Lua benchmark (as close as possible to your FH script)
-- Note: Lua arrays are 1-based, so indexing is adjusted.

-- rand01(seed_ref) -> math_random(seed_ref) in FH
-- In Lua we'll use an explicit LCG to emulate "seed passed by reference".
-- seed_ref is a table {seed} so functions can mutate it.

local function rand01(seed_ref)
  -- 32-bit LCG (Numerical Recipes style)
  local s = seed_ref[1] or 0
  s = (1664525 * s + 1013904223) % 4294967296
  seed_ref[1] = s
  return s / 4294967296
end

local function i32(x)
  -- Your FH i32() returns 2 always; keep it identical.
  return 2
end

local function make_tilemap(w, h, seed_ref)
  local tiles = {}
  for i = 1, w * h do
    local t = 0
    if rand01(seed_ref) < 0.18 then t = 1 end
    tiles[i] = t
  end
  local m = {}
  m['w'] = w;
  m['h'] = h;
  m['tiles'] = tiles;
  return m
end

local function tile_at(tiles, w, h, x, y)
  -- x,y in FH are 0-based tile coords; bounds treat outside as solid (1).
  if x < 0 or y < 0 then return 1 end
  if x >= w or y >= h then return 1 end
  -- Convert (x,y) -> 1-based index:
  return tiles[(y * w + x) + 1]
end

local function aabb_hits_solid(tiles, w, h, x, y, ew, eh)
  local x0 = i32(x);       local y0 = i32(y)
  local x1 = i32(x + ew);  local y1 = i32(y + eh)

  if tile_at(tiles, w, h, x0, y0) == 1 then return true end
  if tile_at(tiles, w, h, x1, y0) == 1 then return true end
  if tile_at(tiles, w, h, x0, y1) == 1 then return true end
  if tile_at(tiles, w, h, x1, y1) == 1 then return true end
  return false
end

local function spawn_entity(seed_ref, w, h, xs, ys, vxs, vys, ws, hs, onGs, ais, lifes)
  xs[#xs + 1]    = rand01(seed_ref) * (w - 2) + 1
  ys[#ys + 1]    = rand01(seed_ref) * (h - 2) + 1
  vxs[#vxs + 1]  = (rand01(seed_ref) - 0.5) * 6.0
  vys[#vys + 1]  = (rand01(seed_ref) - 0.5) * 2.0
  ws[#ws + 1]    = 0.9
  hs[#hs + 1]    = 0.9
  onGs[#onGs + 1]= false
  ais[#ais + 1]  = (rand01(seed_ref) < 0.25)
  lifes[#lifes + 1] = 300 + (rand01(seed_ref) * 600)
end

local function bench(frames, entities, spawnPerFrame)
  local seed = { 123456789 }
  local map = make_tilemap(128, 72, seed)
  local w, h, tiles = map.w, map.h, map.tiles

  local xs, ys, vxs, vys = {}, {}, {}, {}
  local ws, hs, onGs, ais, lifes = {}, {}, {}, {}, {}

  for i = 1, entities do
    spawn_entity(seed, w, h, xs, ys, vxs, vys, ws, hs, onGs, ais, lifes)
  end

  local dt = 1.0 / 60.0
  local checksum = 0.0

  for f = 1, frames do
    for s = 1, spawnPerFrame do
      spawn_entity(seed, w, h, xs, ys, vxs, vys, ws, hs, onGs, ais, lifes)
    end

    -- in-place compaction across ALL arrays
    local write = 1
    local L = #xs

    for n = 1, L do
      local x  = xs[n];  local y  = ys[n]
      local vx = vxs[n]; local vy = vys[n]
      local ew = ws[n];  local eh = hs[n]
      local onG = onGs[n]; local ai = ais[n]
      local life = lifes[n]

      vy = vy + 18.0 * dt

      if ai then
        local dir = -1
        if vx >= 0 then dir = 1 end
        local aheadX = i32(x + dir)
        local footY  = i32(y + 1.0)
        if tile_at(tiles, w, h, aheadX, footY) == 0 then
          vx = -vx
        end
      end

      local newX = x + vx * dt
      if not aabb_hits_solid(tiles, w, h, newX, y, ew, eh) then
        x = newX
      else
        vx = -vx * 0.3
      end

      local newY = y + vy * dt
      if not aabb_hits_solid(tiles, w, h, x, newY, ew, eh) then
        y = newY
        onG = false
      else
        if vy > 0 then onG = true end
        vy = 0
      end

      if onG then
        vx = vx * (1.0 - 8.0 * dt)
      end

      life = life - 1
      checksum = checksum + x + y + vx + vy

      if life > 0 then
        xs[write] = x;   ys[write] = y
        vxs[write] = vx; vys[write] = vy
        ws[write] = ew;  hs[write] = eh
        onGs[write] = onG; ais[write] = ai
        lifes[write] = life
        write = write + 1
      end
    end

    -- trim tails (set to nil so GC can drop refs if needed)
    for k = write, L do
      xs[k] = nil; ys[k] = nil; vxs[k] = nil; vys[k] = nil
      ws[k] = nil; hs[k] = nil; onGs[k] = nil; ais[k] = nil; lifes[k] = nil
    end
  end

  io.write(string.format("%f\n", checksum))
end

local function main()
  bench(3600, 300, 3)
end

main()