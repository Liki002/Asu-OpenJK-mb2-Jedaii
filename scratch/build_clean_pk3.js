const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');

const repoDir = process.cwd();
const sourceDir = path.join(repoDir, 'pk3_ranked_rpg');

const outDir = path.join(repoDir, 'build', 'Release');
fs.mkdirSync(outDir, { recursive: true });

const tempZip = path.join(repoDir, 'scratch', 'temp_rpg_hud.zip');
const rootPk3 = path.join(repoDir, 'zzz_rpg_hud.pk3');
const releasePk3 = path.join(outDir, 'zzz_rpg_hud.pk3');

try {
    if (fs.existsSync(tempZip)) fs.unlinkSync(tempZip);
} catch (e) {}

try {
    if (fs.existsSync(rootPk3)) fs.unlinkSync(rootPk3);
} catch (e) {}

try {
    if (fs.existsSync(releasePk3)) fs.unlinkSync(releasePk3);
} catch (e) {
    console.warn('Warning: Release directory PK3 is locked/busy. Make sure your JKA game/server is closed.');
}

console.log('Packaging pk3_ranked_rpg...');

// Use Powershell to compress gfx and shaders inside pk3_ranked_rpg
const psCmd = `powershell -Command "Set-Location '${sourceDir.replace(/\\/g, '\\\\')}'; Compress-Archive -Path 'gfx','shaders' -DestinationPath '${tempZip.replace(/\\/g, '\\\\')}' -Force"`;
execSync(psCmd);

// Copy to destinations
try {
    fs.copyFileSync(tempZip, rootPk3);
    console.log(`- Created root package: ${rootPk3}`);
} catch (e) {
    console.error(`Error writing root PK3: ${e.message}`);
}

try {
    fs.copyFileSync(tempZip, releasePk3);
    console.log(`- Created release package: ${releasePk3}`);
} catch (e) {
    console.warn(`Warning: Could not copy to release directory (file locked). Close JKA first.`);
}

try {
    fs.unlinkSync(tempZip);
} catch (e) {}

console.log(`Success! zzz_rpg_hud.pk3 created at:\n- ${rootPk3}\n- ${releasePk3}`);
