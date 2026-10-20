const { spawnSync } = require('child_process');
const { status, stdout, stderr } = spawnSync(process.execPath, [
  '-p',
  `JSON.stringify([
    typeof process.permission,
    process.permission.has("fs.read"),
    process.permission.has("fs.write"),
    process.permission.has("child"),
    process.permission.has("worker"),
  ])`,
]);
console.log(JSON.stringify({
  status,
  stdout: stdout.toString().trim(),
  stderr: stderr.toString(),
}));
