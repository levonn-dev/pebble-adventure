#!/usr/bin/env node
// Convert fox_idle_0.png (24x24) into a 25x25 menu icon for Pebble.
// Centers the sprite on a transparent 25x25 canvas.

const sharp = require('sharp');
const path = require('path');

const SRC = path.join(__dirname, '..', 'resources', 'images', 'fox_idle_0.png');
const DST = path.join(__dirname, '..', 'resources', 'images', 'app_icon.png');

sharp(SRC)
  .extend({
    top: 0,
    bottom: 1,
    left: 0,
    right: 1,
    background: { r: 0, g: 0, b: 0, alpha: 0 }
  })
  .toFile(DST)
  .then(info => {
    console.log(`Wrote ${DST} (${info.width}x${info.height}, ${info.size} bytes)`);
  })
  .catch(err => {
    console.error(err);
    process.exit(1);
  });
