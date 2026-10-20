'use strict';
const fs = require('fs');
const path = require('path');
const assert = require('assert');
const os = require('os');
const { Worker, isMainThread, parentPort } = require('worker_threads');

if (!isMainThread) {
  // A worker mounts the archive too, at the same mount point, so the main
  // script and the code next to it can be loaded from there.
  parentPort.postMessage({
    dirname: __dirname,
    greeting: fs.readFileSync(path.join(__dirname, 'data', 'greeting.txt'), 'utf8'),
    sum: require('./modules/math.js').add(2, 3),
  });
  return;
}

// With "useVfsZip", the bundled assets are stored as a single ZIP archive
// and mounted through the ZipProvider. Everything is reachable through
// __dirname-relative paths and relative requires, like the plain SEA VFS.

// The main script runs from inside the VFS, not from the executable.
assert.notStrictEqual(__filename, process.execPath);
assert.strictEqual(path.basename(__filename), 'sea.js');
assert.strictEqual(require.main, module);
console.log('main script runs from', __filename);

// The archive is mounted at the mount point reserved for --vfs-load.
assert.strictEqual(__dirname, path.join(os.devNull, 'vfs', '0'));

// The main script itself is readable through fs.
assert.strictEqual(fs.existsSync(__filename), true);

// Read the config file through standard fs (decompressed from the archive).
const config = JSON.parse(
  fs.readFileSync(path.join(__dirname, 'config.json'), 'utf8'));
assert.strictEqual(config.name, 'test-app');
assert.strictEqual(config.version, '1.0.0');

// Read a nested text file.
const greeting = fs.readFileSync(
  path.join(__dirname, 'data', 'greeting.txt'), 'utf8');
assert.strictEqual(greeting, 'Hello from SEA VFS!');

// existsSync and statSync work, including for implicit directories.
assert.strictEqual(fs.existsSync(path.join(__dirname, 'nonexistent.txt')), false);
assert.strictEqual(fs.statSync(path.join(__dirname, 'config.json')).isFile(), true);
assert.strictEqual(fs.statSync(path.join(__dirname, 'data')).isDirectory(), true);

// readdirSync lists archive entries and the injected main script.
const entries = fs.readdirSync(__dirname);
assert.ok(entries.includes('config.json'));
assert.ok(entries.includes('data'));
assert.ok(entries.includes('sea.js'));

// Relative require of a module stored (deflated) in the archive.
const math = require('./modules/math.js');
assert.strictEqual(math.add(2, 3), 5);

// Bare specifier lookup inside the mount.
const pkg = require('test-pkg');
assert.strictEqual(pkg.greet('SEA'), 'Hello, SEA!');

// node:sea assets are replaced by the archive in zip mode; the VFS is the
// way to read individual assets.
const sea = require('node:sea');
assert.strictEqual(sea.isSea(), true);

// Repeated reads return independent buffers.
const a = fs.readFileSync(path.join(__dirname, 'data', 'greeting.txt'));
const b = fs.readFileSync(path.join(__dirname, 'data', 'greeting.txt'));
assert.notStrictEqual(a, b);
a[0] = 0;
assert.strictEqual(b.toString('utf8'), 'Hello from SEA VFS!');

// The mount point is taken by the archive, so a worker cannot --vfs-load.
assert.throws(() => new Worker(__filename, {
  execArgv: ['--experimental-vfs', `--vfs-load=${__dirname}`],
}), { code: 'ERR_WORKER_INVALID_EXEC_ARGV' });

const worker = new Worker(__filename);
worker.once('message', (message) => {
  assert.deepStrictEqual(message, {
    dirname: __dirname,
    greeting: 'Hello from SEA VFS!',
    sum: 5,
  });
  console.log('All SEA VFS zip tests passed!');
});
