const fs = require('fs');
const path = require('path');
const zlib = require('zlib');

function decodePNG(buffer) {
    if (buffer.readUInt32BE(0) !== 0x89504E47 || buffer.readUInt32BE(4) !== 0x0D0A1A0A) {
        throw new Error('Not a valid PNG file');
    }

    let pos = 8;
    let width = 0, height = 0, bitDepth = 0, colorType = 0;
    const idatChunks = [];

    while (pos < buffer.length) {
        const length = buffer.readUInt32BE(pos);
        const type = buffer.toString('ascii', pos + 4, pos + 8);
        const data = buffer.subarray(pos + 8, pos + 8 + length);
        pos += 12 + length;

        if (type === 'IHDR') {
            width = data.readUInt32BE(0);
            height = data.readUInt32BE(4);
            bitDepth = data[8];
            colorType = data[9];
        } else if (type === 'IDAT') {
            idatChunks.push(data);
        } else if (type === 'IEND') {
            break;
        }
    }

    if (bitDepth !== 8 || (colorType !== 6 && colorType !== 2)) {
        throw new Error(`Unsupported PNG format: bitDepth=${bitDepth}, colorType=${colorType}`);
    }

    const bpp = colorType === 6 ? 4 : 3;
    const decompressed = zlib.inflateSync(Buffer.concat(idatChunks));
    const stride = width * bpp;
    const rgbaBuffer = Buffer.alloc(width * height * 4);

    let srcPos = 0;
    const prevScanline = Buffer.alloc(stride);

    for (let y = 0; y < height; y++) {
        const filterType = decompressed[srcPos++];
        const currScanline = Buffer.alloc(stride);

        for (let x = 0; x < stride; x++) {
            const raw = decompressed[srcPos++];
            const a = x >= bpp ? currScanline[x - bpp] : 0;
            const b = prevScanline[x];
            const c = x >= bpp ? prevScanline[x - bpp] : 0;

            let val = raw;
            if (filterType === 1) { // Sub
                val = (raw + a) & 0xFF;
            } else if (filterType === 2) { // Up
                val = (raw + b) & 0xFF;
            } else if (filterType === 3) { // Average
                val = (raw + Math.floor((a + b) / 2)) & 0xFF;
            } else if (filterType === 4) { // Paeth
                const p = a + b - c;
                const pa = Math.abs(p - a);
                const pb = Math.abs(p - b);
                const pc = Math.abs(p - c);
                let pr = a;
                if (pb < pa && pb < pc) pr = b;
                else if (pc < pa) pr = c;
                val = (raw + pr) & 0xFF;
            }
            currScanline[x] = val;
        }

        currScanline.copy(prevScanline);

        // Convert to RGBA
        const rowDst = y * width * 4;
        for (let px = 0; px < width; px++) {
            const dstOffset = rowDst + px * 4;
            if (colorType === 6) {
                rgbaBuffer[dstOffset + 0] = currScanline[px * 4 + 0];
                rgbaBuffer[dstOffset + 1] = currScanline[px * 4 + 1];
                rgbaBuffer[dstOffset + 2] = currScanline[px * 4 + 2];
                rgbaBuffer[dstOffset + 3] = currScanline[px * 4 + 3];
            } else {
                rgbaBuffer[dstOffset + 0] = currScanline[px * 3 + 0];
                rgbaBuffer[dstOffset + 1] = currScanline[px * 3 + 1];
                rgbaBuffer[dstOffset + 2] = currScanline[px * 3 + 2];
                rgbaBuffer[dstOffset + 3] = 255;
            }
        }
    }

    return { width, height, rgba: rgbaBuffer };
}

function rgbaToTGA(width, height, rgbaBuffer) {
    const header = Buffer.alloc(18);
    header[2] = 2; // Uncompressed true-color
    header.writeUInt16LE(width, 12);
    header.writeUInt16LE(height, 14);
    header[16] = 32; // 32 bpp
    header[17] = 0x20; // Top-to-bottom

    const pixelData = Buffer.alloc(width * height * 4);
    const totalPixels = width * height;

    for (let i = 0; i < totalPixels; i++) {
        const offset = i * 4;
        // RGBA -> BGRA
        pixelData[offset + 0] = rgbaBuffer[offset + 2]; // B
        pixelData[offset + 1] = rgbaBuffer[offset + 1]; // G
        pixelData[offset + 2] = rgbaBuffer[offset + 0]; // R
        pixelData[offset + 3] = rgbaBuffer[offset + 3]; // A
    }

    return Buffer.concat([header, pixelData]);
}

const inputDir = path.join(__dirname, 'NewAssets', 'playing_cards_52');
const targetCardsDir = path.join(__dirname, '..', 'pk3_ranked_rpg', 'gfx', 'rpg_hud', 'cards');

if (!fs.existsSync(targetCardsDir)) {
    fs.mkdirSync(targetCardsDir, { recursive: true });
}

const files = fs.readdirSync(inputDir).filter(f => f.endsWith('.png'));
console.log(`Found ${files.length} PNG cards to convert to TGA in ${inputDir}`);

let count = 0;
for (const file of files) {
    const pngPath = path.join(inputDir, file);
    const tgaFilename = file.replace(/\.png$/i, '.tga');
    const tgaPath = path.join(targetCardsDir, tgaFilename);

    try {
        const pngBuf = fs.readFileSync(pngPath);
        const { width, height, rgba } = decodePNG(pngBuf);
        const tgaBuf = rgbaToTGA(width, height, rgba);
        fs.writeFileSync(tgaPath, tgaBuf);
        count++;
    } catch (err) {
        console.error(`Error converting ${file}:`, err);
    }
}

console.log(`Successfully converted ${count}/${files.length} cards to TGA into ${targetCardsDir}!`);
