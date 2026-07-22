const fs = require('fs');
const path = require('path');

function createTGA32(width, height, drawPixelFn) {
    const header = Buffer.alloc(18);
    header[2] = 2; // Uncompressed True-Color
    header.writeUInt16LE(width, 12);
    header.writeUInt16LE(height, 14);
    header[16] = 32; // 32 bits per pixel
    header[17] = 0x28; // Top-left origin, 8-bit alpha

    const pixelBuffer = Buffer.alloc(width * height * 4);

    for (let y = 0; y < height; y++) {
        for (let x = 0; x < width; x++) {
            const index = (y * width + x) * 4;
            const [r, g, b, a] = drawPixelFn(x, y, width, height);
            pixelBuffer[index] = b;     // Blue
            pixelBuffer[index + 1] = g; // Green
            pixelBuffer[index + 2] = r; // Red
            pixelBuffer[index + 3] = a; // Alpha
        }
    }

    return Buffer.concat([header, pixelBuffer]);
}

// 1. Generate HD 512x128 rpg_hud_box.tga
const hudBoxBuffer = createTGA32(512, 128, (x, y, w, h) => {
    const margin = 4;
    const r = 16; // Rounded corner radius
    const innerW = w - margin * 2;
    const innerH = h - margin * 2;

    const px = x - margin;
    const py = y - margin;

    if (px < 0 || py < 0 || px >= innerW || py >= innerH) {
        return [0, 0, 0, 0];
    }

    // Distance to rounded rectangle boundary
    let dx = 0;
    if (px < r) dx = r - px;
    else if (px > innerW - r) dx = px - (innerW - r);

    let dy = 0;
    if (py < r) dy = r - py;
    else if (py > innerH - r) dy = py - (innerH - r);

    const dist = Math.sqrt(dx * dx + dy * dy);

    // Outside rounded corner
    if (dist > r) {
        // Anti-aliased outer edge
        if (dist < r + 1.5) {
            const alphaFactor = 1.0 - (dist - r) / 1.5;
            return [0, 180, 255, Math.floor(200 * alphaFactor)];
        }
        return [0, 0, 0, 0];
    }

    // 2px Glowing Cyan Border
    if (dist > r - 3 || px <= 2 || px >= innerW - 3 || py <= 2 || py >= innerH - 3) {
        return [0, 180, 255, 230]; // Glowing cyan border
    }

    // 1px Subtle Inner Glow
    if (dist > r - 5 || px <= 4 || px >= innerW - 5 || py <= 4 || py >= innerH - 5) {
        return [0, 100, 180, 120];
    }

    // Dark Translucent Glass Fill (35% opacity)
    return [8, 16, 32, 110];
});

// 2. Generate HD 512x64 rpg_bar_bg.tga
const barBgBuffer = createTGA32(512, 64, (x, y, w, h) => {
    const r = 8;
    let dx = 0;
    if (x < r) dx = r - x;
    else if (x > w - r) dx = x - (w - r);

    let dy = 0;
    if (y < r) dy = r - y;
    else if (y > h - r) dy = y - (h - r);

    const dist = Math.sqrt(dx * dx + dy * dy);

    if (dist > r) return [0, 0, 0, 0];

    // Border (1.5px cyan border)
    if (dist > r - 2 || x <= 1 || x >= w - 2 || y <= 1 || y >= h - 2) {
        return [0, 150, 240, 180];
    }

    // Dark progress bar track fill
    return [4, 8, 18, 210];
});

// 3. Generate HD 512x64 rpg_bar_fill.tga
const barFillBuffer = createTGA32(512, 64, (x, y, w, h) => {
    const r = 6;
    let dx = 0;
    if (x < r) dx = r - x;
    else if (x > w - r) dx = x - (w - r);

    let dy = 0;
    if (y < r) dy = r - y;
    else if (y > h - r) dy = y - (h - r);

    const dist = Math.sqrt(dx * dx + dy * dy);

    if (dist > r) return [0, 0, 0, 0];

    // Glowing cyan horizontal gradient fill
    const factor = x / w;
    const red = Math.floor(0 + factor * 20);
    const green = Math.floor(180 + factor * 40);
    const blue = 255;

    return [red, green, blue, 240];
});

// Write to tools/gfx/hud
const hudDir = path.join(__dirname, '..', 'tools', 'gfx', 'hud');
fs.mkdirSync(hudDir, { recursive: true });

fs.writeFileSync(path.join(hudDir, 'rpg_hud_box.tga'), hudBoxBuffer);
fs.writeFileSync(path.join(hudDir, 'rpg_bar_bg.tga'), barBgBuffer);
fs.writeFileSync(path.join(hudDir, 'rpg_bar_fill.tga'), barFillBuffer);

console.log('HD 32-bit TGA assets generated successfully!');
