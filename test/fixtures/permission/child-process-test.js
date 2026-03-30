const { spawnSync } = require('child_process');
spawnSync(process.execPath, ['--version']);
