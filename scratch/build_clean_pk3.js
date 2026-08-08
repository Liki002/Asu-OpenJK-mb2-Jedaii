const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');

const repoDir = process.cwd();
const tempBuildDir = path.join(repoDir, 'scratch', 'clean_pk3_root');

// Clean temporary directory
if (fs.existsSync(tempBuildDir)) {
    fs.rmSync(tempBuildDir, { recursive: true, force: true });
}

const hudTarget = path.join(tempBuildDir, 'gfx', 'hud');
const rpgHudTarget = path.join(tempBuildDir, 'gfx', 'rpg_hud');
const shaderTarget = path.join(tempBuildDir, 'shaders');

fs.mkdirSync(hudTarget, { recursive: true });
fs.mkdirSync(rpgHudTarget, { recursive: true });
fs.mkdirSync(shaderTarget, { recursive: true });

// Copy gfx/hud files
const sourceHud = path.join(repoDir, 'tools', 'gfx', 'hud');
const hudFiles = [
    'avatar_default.jpg',
    'avatar_sith.jpg',
    'rpg_hud_box.tga',
    'rpg_bar_bg.tga',
    'rpg_bar_fill.tga',
    'potato.tga'
];

for (const file of hudFiles) {
    const src = path.join(sourceHud, file);
    if (fs.existsSync(src)) {
        fs.copyFileSync(src, path.join(hudTarget, file));
        console.log(`Copied gfx/hud/${file}`);
    } else {
        console.error(`Missing gfx/hud/${file}`);
    }
}

// Copy gfx/rpg_hud files
const sourceRpgHud = path.join(repoDir, 'tools', 'gfx', 'rpg_hud');
if (fs.existsSync(sourceRpgHud)) {
    const rpgFiles = fs.readdirSync(sourceRpgHud);
    for (const file of rpgFiles) {
        fs.copyFileSync(path.join(sourceRpgHud, file), path.join(rpgHudTarget, file));
        console.log(`Copied gfx/rpg_hud/${file}`);
    }
}

// Copy shader file
const sourceShader = path.join(repoDir, 'tools', 'shaders', 'rpg_hud.shader');
if (fs.existsSync(sourceShader)) {
    fs.copyFileSync(sourceShader, path.join(shaderTarget, 'rpg_hud.shader'));
    console.log('Copied rpg_hud.shader');
}

// Package into clean zz_JedaiiRankedRPG.pk3
const outDir = path.join(repoDir, 'build', 'Release');
fs.mkdirSync(outDir, { recursive: true });
const outZip = path.join(outDir, 'zz_JedaiiRankedRPG.zip');
const outPk3 = path.join(outDir, 'zz_JedaiiRankedRPG.pk3');

if (fs.existsSync(outPk3)) fs.unlinkSync(outPk3);
if (fs.existsSync(outZip)) fs.unlinkSync(outZip);

const psCmd = `powershell -Command "Set-Location '${tempBuildDir.replace(/\\/g, '\\\\')}'; Compress-Archive -Path 'gfx','shaders' -DestinationPath '${outZip.replace(/\\/g, '\\\\')}' -Force; Move-Item '${outZip.replace(/\\/g, '\\\\')}' '${outPk3.replace(/\\/g, '\\\\')}' -Force"`;
execSync(psCmd);

console.log(`Clean lightweight zz_JedaiiRankedRPG.pk3 successfully created at: ${outPk3}`);

