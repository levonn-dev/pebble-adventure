// Convert color biome backgrounds to B&W versions for Flint (1-bit display).
// Each biome has tuned threshold and negate settings to produce the best
// silhouette on a black background.
//
// Usage: node tools/convert_bw_backgrounds.js

const path = require('path');
const sharp = require(path.join(__dirname, '..', 'node_modules', 'sharp'));

const colorDir = path.join(__dirname, '..', 'resources', 'images', 'backgrounds', 'color');
const bwDir = path.join(__dirname, '..', 'resources', 'images', 'backgrounds', 'bw');

// Per-biome conversion settings:
//   threshold: grayscale cutoff (0-255) for B&W
//   negate: true = dark terrain on light sky becomes white terrain on black sky
//           false = keep bright areas white (for scenes that are already dark)
//   channel: extract a single color channel instead of grayscale (optional)
//   dither: use dithering instead of hard threshold (optional, 0.0-1.0)
const biomes = {
  plains:   { threshold: 128, negate: true },
  forest:   { threshold: 86,  negate: true },
  water:    { channel: 'red', dither: 1.0 },
  mountain: { threshold: 180, negate: true },
  cave:     { threshold: 128, negate: false },
  storm:    { threshold: 120, negate: false },
};

async function convert(name, opts) {
  const input = path.join(colorDir, `bg_${name}.png`);
  const output = path.join(bwDir, `bg_${name}_bw.png`);

  let pipeline = sharp(input);

  if (opts.channel) {
    pipeline = pipeline.extractChannel(opts.channel);
  } else {
    pipeline = pipeline.grayscale();
  }

  pipeline = pipeline.resize(144, null, { fit: 'inside', withoutEnlargement: false });

  if (opts.dither != null) {
    // Dithered: skip hard threshold, let png quantization dither
    if (opts.negate) pipeline = pipeline.negate();
    await pipeline
      .png({ colours: 2, dither: opts.dither })
      .toFile(output);
  } else {
    // Hard threshold
    pipeline = pipeline.threshold(opts.threshold);
    if (opts.negate) pipeline = pipeline.negate();
    await pipeline
      .png({ colours: 2 })
      .toFile(output);
  }

  console.log(`${name}: done (${JSON.stringify(opts)})`);
}

(async () => {
  for (const [name, opts] of Object.entries(biomes)) {
    await convert(name, opts);
  }
  console.log('All B&W backgrounds generated.');
})();
