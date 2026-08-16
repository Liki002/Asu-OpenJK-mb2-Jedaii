const fs = require('fs');
const path = require('path');

const repoDir = process.cwd();
const outDir = path.join(repoDir, 'pk3_ranked_rpg', 'gfx', 'rpg_hud');
fs.mkdirSync(outDir, { recursive: true });

// Helper to create 32-bit uncompressed TGA buffer (RGBA)
function create32BitTGA(width, height, drawPixelFn) {
	const headerLen = 18;
	const pixelBytes = width * height * 4;
	const buf = Buffer.alloc(headerLen + pixelBytes);

	buf[2] = 2; // Uncompressed True-Color
	buf[12] = width & 0xff;
	buf[13] = (width >> 8) & 0xff;
	buf[14] = height & 0xff;
	buf[15] = (height >> 8) & 0xff;
	buf[16] = 32; // 32 bits per pixel
	buf[17] = 8;  // 8 alpha bits, top-left origin

	let offset = headerLen;
	for (let y = height - 1; y >= 0; y--) { // TGA bottom-up scanlines
		for (let x = 0; x < width; x++) {
			const rgba = drawPixelFn(x, y, width, height);
			buf[offset]     = rgba.b;
			buf[offset + 1] = rgba.g;
			buf[offset + 2] = rgba.r;
			buf[offset + 3] = rgba.a;
			offset += 4;
		}
	}
	return buf;
}

// 1. Generate avatar_default.tga (512x512) - Perfect Circular Frame with 100% Transparent Alpha outside ring
const avatarBuf = create32BitTGA(512, 512, (x, y, w, h) => {
	const cx = w / 2;
	const cy = h / 2;
	const radius = 240;
	const dx = x - cx;
	const dy = y - cy;
	const dist = Math.sqrt(dx * dx + dy * dy);

	// Outside circle -> 100% transparent
	if (dist > radius) {
		return { r: 0, g: 0, b: 0, a: 0 };
	}

	// Glowing cyan ring edge (radius - 4 to radius)
	if (dist >= radius - 6) {
		const alpha = Math.floor(255 * (1 - (dist - (radius - 6)) / 6));
		return { r: 0, g: 200, b: 255, a: 220 };
	}

	// Inner ring border line (radius - 8 to radius - 6)
	if (dist >= radius - 10) {
		return { r: 0, g: 140, b: 200, a: 180 };
	}

	// Inside circle background: subtle translucent dark blue/black
	const innerDistRatio = dist / (radius - 10);
	const bgDarkness = Math.floor(15 + (1 - innerDistRatio) * 20);

	// Minimalist Vector Hooded Jedi Silhouette & Saber
	// Center silhouette coordinates relative to cx, cy
	const nx = dx / radius;
	const ny = dy / radius;

	// Lightsaber blade (diagonal glowing cyan line)
	const saberDist = Math.abs(dx * 0.707 - dy * 0.707 + 10);
	if (saberDist < 6 && dy < 80 && dy > -180) {
		return { r: 0, g: 220, b: 255, a: 255 };
	}

	// Hood top head arc
	if (ny >= -0.65 && ny <= -0.25 && Math.abs(nx) <= 0.28) {
		return { r: 240, g: 240, b: 245, a: 240 };
	}
	// Robe body shoulders/torso
	if (ny >= -0.25 && ny <= 0.60 && Math.abs(nx) <= (0.28 + (ny + 0.25) * 0.45)) {
		return { r: 230, g: 230, b: 235, a: 235 };
	}

	// Background fill inside circle
	return { r: 6, g: 12, b: 22, a: 180 };
});

fs.writeFileSync(path.join(outDir, 'avatar_default.tga'), avatarBuf);
fs.writeFileSync(path.join(outDir, 'avatar_default.jpg'), avatarBuf);
console.log('Generated circular avatar_default.tga with transparent alpha background!');

// 2. Generate rpg_hud_box.tga (512x128) - Ultra-subtle MBII dark glass capsule (25% dark alpha)
const boxBuf = create32BitTGA(512, 128, (x, y, w, h) => {
	const r = 24; // Smooth rounded corners
	let dx = 0;
	let dy = 0;
	if (x < r) dx = r - x;
	else if (x > w - r) dx = x - (w - r);
	if (y < r) dy = r - y;
	else if (y > h - r) dy = y - (h - r);

	const dist = Math.sqrt(dx * dx + dy * dy);
	if (dist > r) {
		return { r: 0, g: 0, b: 0, a: 0 };
	}

	// Thin 1.5px glowing cyan border
	if (dist >= r - 2.5) {
		return { r: 0, g: 180, b: 255, a: 140 };
	}

	// Edge highlight
	if (y <= 2 || x <= 2 || x >= w - 3 || y >= h - 3) {
		return { r: 0, g: 160, b: 240, a: 110 };
	}

	// Inner dark glass body (25% opacity matching MBII UI)
	return { r: 4, g: 10, b: 20, a: 65 };
});

fs.writeFileSync(path.join(outDir, 'rpg_hud_box.tga'), boxBuf);
console.log('Generated subtle rpg_hud_box.tga!');

// 3. Generate rpg_bar_bg.tga (512x64) - Dark track with subtle cyan border
const barBgBuf = create32BitTGA(512, 64, (x, y, w, h) => {
	const r = 12;
	let dx = 0, dy = 0;
	if (x < r) dx = r - x;
	else if (x > w - r) dx = x - (w - r);
	if (y < r) dy = r - y;
	else if (y > h - r) dy = y - (h - r);

	const dist = Math.sqrt(dx * dx + dy * dy);
	if (dist > r) return { r: 0, g: 0, b: 0, a: 0 };

	if (dist >= r - 2) {
		return { r: 0, g: 140, b: 220, a: 120 };
	}
	return { r: 2, g: 6, b: 14, a: 180 };
});

fs.writeFileSync(path.join(outDir, 'rpg_bar_bg.tga'), barBgBuf);

// 4. Generate rpg_bar_fill.tga (512x64) - Glowing Cyan progress fill
const fillBuf = create32BitTGA(512, 64, (x, y, w, h) => {
	const ratio = x / w;
	const cyanR = Math.floor(0 + ratio * 20);
	const cyanG = Math.floor(180 + ratio * 40);
	const cyanB = Math.floor(240 + ratio * 15);
	return { r: cyanR, g: cyanG, b: cyanB, a: 220 };
});

fs.writeFileSync(path.join(outDir, 'rpg_bar_fill.tga'), fillBuf);
console.log('Generated rpg_bar_bg.tga and rpg_bar_fill.tga!');

// Package PK3
require('./build_clean_pk3.js');
