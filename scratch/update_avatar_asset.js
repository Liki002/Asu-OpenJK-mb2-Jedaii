const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');

const repoDir = 'c:\\Users\\GæstPC\\Desktop\\Jedaii Servers\\scratch\\Asu-OpenJK-mb2-Jedaii';
const generatedAvatar = 'C:\\Users\\GæstPC\\.gemini\\antigravity\\brain\\43a8bfa1-aab7-417f-9959-455887b600f6\\minimalist_jedi_avatar_1784760864729.jpg';
const targetDir = path.join(repoDir, 'tools', 'gfx', 'hud');

fs.mkdirSync(targetDir, { recursive: true });

// Copy generated avatar as avatar_default.jpg and avatar_default.tga
fs.copyFileSync(generatedAvatar, path.join(targetDir, 'avatar_default.jpg'));
fs.copyFileSync(generatedAvatar, path.join(targetDir, 'avatar_default.tga'));

console.log('Minimalist Jedi Avatar copied to tools/gfx/hud/avatar_default.jpg');

// Run build_clean_pk3.js to update zz_JedaiiRankedRPG.pk3
require('./build_clean_pk3.js');
