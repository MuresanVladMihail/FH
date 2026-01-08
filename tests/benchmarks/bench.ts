// bench.ts
// Run:  npx tsx bench.ts
// or:   npx ts-node bench.ts
// or:   tsc bench.ts && node bench.js

type TileMap = { w: number; h: number; tiles: number[] };

function rand01(seedRef: { v: number }): number {
  // fast LCG, deterministic, returns [0,1)
  // Constants from Numerical Recipes / common LCGs.
  seedRef.v = (seedRef.v * 1664525 + 1013904223) >>> 0;
  return seedRef.v / 0x100000000;
}

function i32(x: number): number {
  // In your FH sample i32(x) returns 2 (stub).
  // To preserve *exact semantics* of that program, keep it constant:
  return 2;

  // If you meant int truncation, use instead:
  // return x | 0;
}

function makeTilemap(w: number, h: number, seed: { v: number }): TileMap {
  const tiles: number[] = new Array(w * h);
  for (let i = 0; i < w * h; i++) {
    let t = 0;
    if (rand01(seed) < 0.18) t = 1;
    tiles[i] = t;
  }
  return { w, h, tiles };
}

function tileAt(tiles: number[], w: number, h: number, x: number, y: number): number {
  if (x < 0 || y < 0) return 1;
  if (x >= w || y >= h) return 1;
  return tiles[y * w + x] | 0;
}

function aabbHitsSolid(
  tiles: number[],
  w: number,
  h: number,
  x: number,
  y: number,
  ew: number,
  eh: number
): boolean {
  const x0 = i32(x);
  const y0 = i32(y);
  const x1 = i32(x + ew);
  const y1 = i32(y + eh);
  if (tileAt(tiles, w, h, x0, y0) === 1) return true;
  if (tileAt(tiles, w, h, x1, y0) === 1) return true;
  if (tileAt(tiles, w, h, x0, y1) === 1) return true;
  if (tileAt(tiles, w, h, x1, y1) === 1) return true;
  return false;
}

function spawnEntity(
  seed: { v: number },
  w: number,
  h: number,
  xs: (number | null)[],
  ys: (number | null)[],
  vxs: (number | null)[],
  vys: (number | null)[],
  ws: (number | null)[],
  hs: (number | null)[],
  onGs: (boolean | null)[],
  ais: (boolean | null)[],
  lifes: (number | null)[]
): void {
  xs.push(rand01(seed) * (w - 2) + 1);
  ys.push(rand01(seed) * (h - 2) + 1);
  vxs.push((rand01(seed) - 0.5) * 6.0);
  vys.push((rand01(seed) - 0.5) * 2.0);
  ws.push(0.9);
  hs.push(0.9);
  onGs.push(false);
  ais.push(rand01(seed) < 0.25);
  lifes.push(300 + rand01(seed) * 600);
}

function bench(frames: number, entities: number, spawnPerFrame: number): void {
  const seed = { v: 123456789 >>> 0 };

  const map = makeTilemap(128, 72, seed);
  const w = map.w;
  const h = map.h;
  const tiles = map.tiles;

  const xs: (number | null)[] = [];
  const ys: (number | null)[] = [];
  const vxs: (number | null)[] = [];
  const vys: (number | null)[] = [];
  const ws: (number | null)[] = [];
  const hs: (number | null)[] = [];
  const onGs: (boolean | null)[] = [];
  const ais: (boolean | null)[] = [];
  const lifes: (number | null)[] = [];

  let i = 0;
  while (i < entities) {
    spawnEntity(seed, w, h, xs, ys, vxs, vys, ws, hs, onGs, ais, lifes);
    i++;
  }

  const dt = 1.0 / 60.0;
  let checksum = 0.0;

  let f = 0;
  while (f < frames) {
    let s = 0;
    while (s < spawnPerFrame) {
      spawnEntity(seed, w, h, xs, ys, vxs, vys, ws, hs, onGs, ais, lifes);
      s++;
    }

    // in-place compaction across ALL arrays
    let write = 0;
    let n = 0;
    const L = xs.length;

    while (n < L) {
      // Unwrap (they should not be null in active region)
      let x = xs[n] as number;
      let y = ys[n] as number;
      let vx = vxs[n] as number;
      let vy = vys[n] as number;
      const ew = ws[n] as number;
      const eh = hs[n] as number;
      let onG = onGs[n] as boolean;
      const ai = ais[n] as boolean;
      let life = lifes[n] as number;

      vy = vy + 18.0 * dt;

      if (ai) {
        let dir = -1;
        if (vx >= 0) dir = 1;
        const aheadX = i32(x + dir);
        const footY = i32(y + 1.0);
        if (tileAt(tiles, w, h, aheadX, footY) === 0) vx = -vx;
      }

      const newX = x + vx * dt;
      if (!aabbHitsSolid(tiles, w, h, newX, y, ew, eh)) x = newX;
      else vx = -vx * 0.3;

      const newY = y + vy * dt;
      if (!aabbHitsSolid(tiles, w, h, x, newY, ew, eh)) {
        y = newY;
        onG = false;
      } else {
        if (vy > 0) onG = true;
        vy = 0;
      }

      if (onG) vx = vx * (1.0 - 8.0 * dt);
      life = life - 1;

      checksum = checksum + x + y + vx + vy;

      if (life > 0) {
        xs[write] = x;
        ys[write] = y;
        vxs[write] = vx;
        vys[write] = vy;
        ws[write] = ew;
        hs[write] = eh;
        onGs[write] = onG;
        ais[write] = ai;
        lifes[write] = life;
        write = write + 1;
      }

      n++;
    }

    // trim tails (set to NULL so GC can drop refs if needed)
    let k = write;
    while (k < L) {
      xs[k] = null;
      ys[k] = null;
      vxs[k] = null;
      vys[k] = null;
      ws[k] = null;
      hs[k] = null;
      onGs[k] = null;
      ais[k] = null;
      lifes[k] = null;
      k = k + 1;
    }

    f++;
  }

  // Print like printf("%f\n", checksum);
  process.stdout.write(checksum.toFixed(6) + "\n");
}

function main(): void {
  bench(3600, 300, 3);
}

main();