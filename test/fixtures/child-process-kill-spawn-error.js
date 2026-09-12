const { spawn } = require('child_process');
const child = spawn('foo123');
child.on('error', () => {});
if (child.kill() !== false || child.killed !== false) process.exit(1);
