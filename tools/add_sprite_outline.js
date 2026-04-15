// Add a 1px white outline to large fox sprites.
// For each transparent pixel, if any of its 8 neighbours is opaque,
// it becomes fully-opaque white. The original opaque pixels are untouched.
//
// Usage: node tools/add_sprite_outline.js
// Input/output: resources/images/sprites/fox/large/fox_lg_*.png  (edited in place)

const path = require('path');
const fs   = require('fs');
const sharp = require(path.join(__dirname, '..', 'node_modules', 'sharp'));

const SPRITE_DIR = path.join(__dirname, '..', 'resources', 'images', 'sprites', 'fox', 'large');
const ALPHA_THRESHOLD = 10;  // pixels with alpha < this are treated as transparent

async function addOutline(filePath) {
  const { data, info } = await sharp(filePath)
    .ensureAlpha()
    .raw()
    .toBuffer({ resolveWithObject: true });

  const { width, height, channels } = info;   // channels === 4 (RGBA)
  const out = Buffer.from(data);              // copy we write into

  const isOpaque = (x, y) => {
    if (x < 0 || x >= width || y < 0 || y >= height) return false;
    return data[(y * width + x) * channels + 3] >= ALPHA_THRESHOLD;
  };

  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      const idx = (y * width + x) * channels;
      if (data[idx + 3] >= ALPHA_THRESHOLD) continue;  // already opaque — skip

      // Check all 8 neighbours
      const needsOutline =
        isOpaque(x - 1, y - 1) || isOpaque(x, y - 1) || isOpaque(x + 1, y - 1) ||
        isOpaque(x - 1, y)                             || isOpaque(x + 1, y)     ||
        isOpaque(x - 1, y + 1) || isOpaque(x, y + 1) || isOpaque(x + 1, y + 1);

      if (needsOutline) {
        out[idx]     = 255;  // R
        out[idx + 1] = 255;  // G
        out[idx + 2] = 255;  // B
        out[idx + 3] = 255;  // A
      }
    }
  }

  await sharp(out, { raw: { width, height, channels } })
    .png()
    .toFile(filePath);

  console.log(`${path.basename(filePath)}: outline added (${width}×${height})`);
}

(async () => {
  const files = fs.readdirSync(SPRITE_DIR)
    .filter(f => f.startsWith('fox_lg_') && f.endsWith('.png'))
    .sort()
    .map(f => path.join(SPRITE_DIR, f));

  if (files.length === 0) {
    console.error('No fox_lg_*.png files found in', SPRITE_DIR);
    process.exit(1);
  }

  for (const f of files) {
    await addOutline(f);
  }

  console.log('Done.');
})();
