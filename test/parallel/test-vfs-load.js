'use strict';

// Covers --vfs-load: running a mounted directory's entry point with require()
// resolving inside the mount, a provider registered by either a -r (CJS) or an
// --import (ESM) preload backing a non-directory source, a ZIP archive claimed
// by the built-in provider, and a worker reaching the mount, whether it
// inherits the options or is given them itself.
//
// Native addon loading from a mount is not exercised here (it needs a compiled
// .node), only the startup wiring around it.

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { pathToFileURL } = require('url');
const { spawnSync } = require('child_process');

tmpdir.refresh();
let id = 0;
function fixture(name) { return path.join(tmpdir.path, `${id++}-${name}`); }

function run(args) {
  return spawnSync(process.execPath, ['--experimental-vfs', ...args], { encoding: 'utf8' });
}

// Node.js can be built without NODE_OPTIONS support, in which case the
// environment cannot carry a flag at all and there is nothing to assert.
const hasNodeOptions = !process.config.variables.node_without_node_options;

// NODE_OPTIONS is tokenized with shell-like quoting, so a path holding a space
// or a quote - as the checkout directory does on some CI machines - has to be
// quoted and escaped rather than interpolated raw.
function envArg(flag, value) {
  return `"${flag}=${value.replace(/[\\"]/g, '\\$&')}"`;
}

// A directory source: the entry point runs and require() resolves inside it.
{
  const dir = fixture('app');
  fs.mkdirSync(path.join(dir, 'lib'), { recursive: true });
  fs.writeFileSync(path.join(dir, 'index.js'),
                   "console.log(require('./lib/greet')());\n");
  fs.writeFileSync(path.join(dir, 'lib', 'greet.js'),
                   "module.exports = () => 'hello from inside the mount';\n");
  const res = run([`--vfs-load=${dir}`]);
  assert.strictEqual(res.status, 0, res.stderr);
  assert.match(res.stdout, /hello from inside the mount/);
}

// A provider registered by a -r (CommonJS) preload backs a custom file format.
{
  const providerModule = fixture('provider.js');
  fs.writeFileSync(providerModule, `
'use strict';
const fs = require('fs');
const vfs = require('node:vfs');
const MAGIC = Buffer.from('CUSTOMFMT');
vfs.registerProvider({
  name: 'customfmt',
  canHandle(p, stats) {
    if (!stats.isFile()) return false;
    const fd = fs.openSync(p, 'r');
    try {
      const buf = Buffer.alloc(MAGIC.length);
      fs.readSync(fd, buf, 0, MAGIC.length, 0);
      return buf.equals(MAGIC);
    } finally { fs.closeSync(fd); }
  },
  create(p) {
    const body = fs.readFileSync(p).subarray(MAGIC.length).toString('utf8');
    const provider = new vfs.MemoryProvider();
    provider.writeFileSync('/index.js', body);
    return provider;
  },
});
`);
  const target = fixture('app.customfmt');
  fs.writeFileSync(target, Buffer.concat([
    Buffer.from('CUSTOMFMT'),
    Buffer.from("console.log('hello from custom provider');"),
  ]));
  const res = run(['-r', providerModule, `--vfs-load=${target}`]);
  assert.strictEqual(res.status, 0, res.stderr);
  assert.match(res.stdout, /hello from custom provider/);
}

// A provider registered by an --import (ES module) preload: this only works
// because mounting is deferred until after the --import loop has run.
{
  const providerModule = fixture('provider.mjs');
  fs.writeFileSync(providerModule, `
import fs from 'node:fs';
import { registerProvider, MemoryProvider } from 'node:vfs';
const MAGIC = Buffer.from('ESMFMT');
registerProvider({
  name: 'esmfmt',
  canHandle(p, stats) {
    if (!stats.isFile()) return false;
    return fs.readFileSync(p).subarray(0, MAGIC.length).equals(MAGIC);
  },
  create(p) {
    const body = fs.readFileSync(p).subarray(MAGIC.length).toString('utf8');
    const provider = new MemoryProvider();
    provider.writeFileSync('/index.js', body);
    return provider;
  },
});
`);
  const target = fixture('app.esmfmt');
  fs.writeFileSync(target, Buffer.concat([
    Buffer.from('ESMFMT'),
    Buffer.from("console.log('hello from ESM-imported provider');"),
  ]));
  const res = run([
    '--import', pathToFileURL(providerModule).href,
    `--vfs-load=${target}`,
  ]);
  assert.strictEqual(res.status, 0, res.stderr);
  assert.match(res.stdout, /hello from ESM-imported provider/);
}

// A ZIP archive is claimed by the built-in provider (detected by opening it,
// not by extension).
{
  const zlib = require('zlib');
  const zipPath = fixture('app.zip');
  const entry = zlib.ZipEntry.createSync(
    'index.js', Buffer.from("console.log('hello from zip archive');"));
  const chunks = [];
  for (const chunk of zlib.createZipArchiveSync([entry])) chunks.push(chunk);
  fs.writeFileSync(zipPath, Buffer.concat(chunks));
  const res = run([`--vfs-load=${zipPath}`]);
  assert.strictEqual(res.status, 0, res.stderr);
  assert.match(res.stdout, /hello from zip archive/);
}

// A worker inherits the mount, so a worker script that lives inside it
// (addressed here via the entry's own __dirname) resolves and runs.
{
  const dir = fixture('worker-app');
  fs.mkdirSync(dir, { recursive: true });
  fs.writeFileSync(path.join(dir, 'index.js'), `
'use strict';
const path = require('path');
const { Worker } = require('worker_threads');
const w = new Worker(path.join(__dirname, 'worker.js'));
w.on('message', (m) => { console.log(m); process.exit(0); });
w.on('error', (e) => { console.error(e); process.exit(1); });
`);
  fs.writeFileSync(path.join(dir, 'worker.js'), `
'use strict';
require('worker_threads').parentPort.postMessage('hello from worker in mount');
`);
  const res = run([`--vfs-load=${dir}`]);
  assert.strictEqual(res.status, 0, res.stderr);
  assert.match(res.stdout, /hello from worker in mount/);
}

// The same with an --import preload, which defers the worker's mount until the
// preload has run: its entry point is still resolved inside the mount, with the
// extension search a file entry point gets.
{
  const dir = fixture('worker-import-app');
  fs.mkdirSync(dir, { recursive: true });
  fs.writeFileSync(path.join(dir, 'index.js'), `
'use strict';
const path = require('path');
const { Worker } = require('worker_threads');
const w = new Worker(path.join(__dirname, 'worker'));
w.on('message', (m) => { console.log(m); process.exit(0); });
w.on('error', (e) => { console.error(e); process.exit(1); });
`);
  fs.writeFileSync(path.join(dir, 'worker.js'), `
'use strict';
require('worker_threads').parentPort.postMessage('hello from worker in mount');
`);
  const preload = fixture('empty-preload.mjs');
  fs.writeFileSync(preload, '');
  const res = run(['--import', pathToFileURL(preload).href, `--vfs-load=${dir}`]);
  assert.strictEqual(res.status, 0, res.stderr);
  assert.match(res.stdout, /hello from worker in mount/);
}

// The same, for a worker whose nearest package.json inside the mount says
// "module": the entry point's type is read from the mount rather than from
// the real file system above the reserved mount point.
{
  const dir = fixture('worker-esm-app');
  fs.mkdirSync(dir, { recursive: true });
  fs.writeFileSync(path.join(dir, 'package.json'),
                   '{"type":"module","main":"index.js"}\n');
  fs.writeFileSync(path.join(dir, 'index.js'), `
import path from 'node:path';
import { Worker } from 'node:worker_threads';
import { fileURLToPath } from 'node:url';
const here = path.dirname(fileURLToPath(import.meta.url));
const w = new Worker(path.join(here, 'worker.js'));
w.on('message', (m) => { console.log(m); process.exit(0); });
w.on('error', (e) => { console.error(e); process.exit(1); });
`);
  // No extension hint and no CJS wrapper: this only parses if the worker is
  // loaded as ESM, which takes reading "type" out of the mount's package.json.
  fs.writeFileSync(path.join(dir, 'worker.js'), `
import { parentPort } from 'node:worker_threads';
parentPort.postMessage('hello from esm worker in mount');
`);
  const res = run([`--vfs-load=${dir}`]);
  assert.strictEqual(res.status, 0, res.stderr);
  assert.match(res.stdout, /hello from esm worker in mount/);
}

// A worker created with its own execArgv does not inherit the parent's options,
// so it has to be given the mount itself. Because the loaded source is always
// the first file system a thread mounts, it lands at the same reserved mount
// point in both threads, and a worker path the parent built still resolves.
{
  const dir = fixture('worker-execargv-app');
  fs.mkdirSync(dir, { recursive: true });
  fs.writeFileSync(path.join(dir, 'index.js'), `
'use strict';
const path = require('path');
const { Worker } = require('worker_threads');
const w = new Worker(path.join(__dirname, 'worker.js'), {
  execArgv: ['--experimental-vfs', '--vfs-load=' + process.argv[1]],
});
w.on('message', (m) => { console.log(m); process.exit(0); });
w.on('error', (e) => { console.error(e); process.exit(1); });
`);
  fs.writeFileSync(path.join(dir, 'worker.js'), `
'use strict';
require('worker_threads').parentPort.postMessage('worker ran from ' + __dirname);
`);
  // A preload that mounts a file system of its own runs before the --vfs-load
  // source is mounted, and still does not move it.
  const preload = fixture('mounting-preload.js');
  fs.writeFileSync(preload, `
'use strict';
require('node:vfs').create().mount();
`);

  const seen = new Set();
  for (const args of [[`--vfs-load=${dir}`], ['-r', preload, `--vfs-load=${dir}`]]) {
    const res = run(args);
    assert.strictEqual(res.status, 0, res.stderr);
    const [, dirname] = /worker ran from (\S+)/.exec(res.stdout);
    seen.add(dirname);
  }
  // The worker resolved a path the main thread built, in both runs, and that
  // path did not move between them.
  assert.strictEqual(seen.size, 1, [...seen].join());
}

// Without that flag the worker has no mount to load from, so a script in the
// mount cannot be its entry point.
{
  const dir = fixture('worker-execargv-missing');
  fs.mkdirSync(dir, { recursive: true });
  fs.writeFileSync(path.join(dir, 'index.js'), `
'use strict';
const path = require('path');
const { Worker } = require('worker_threads');
const w = new Worker(path.join(__dirname, 'worker.js'), { execArgv: [] });
w.on('message', (m) => { console.log('ran:' + m); process.exit(0); });
w.on('error', (e) => { console.log('failed:' + e.code); process.exit(0); });
`);
  fs.writeFileSync(path.join(dir, 'worker.js'), `
'use strict';
require('worker_threads').parentPort.postMessage('unexpected');
`);
  const res = run([`--vfs-load=${dir}`]);
  assert.strictEqual(res.status, 0, res.stderr);
  assert.match(res.stdout, /failed:/);
  assert.doesNotMatch(res.stdout, /ran:/);
}

// --vfs-load names the source it loads, so it always takes a value.
{
  const res = run(['--vfs-load']);
  assert.notStrictEqual(res.status, 0);
  assert.match(res.stderr, /--vfs-load requires an argument/);
}

// The value may also be given as a separate argument.
{
  const dir = fixture('spaced-value');
  fs.mkdirSync(dir, { recursive: true });
  fs.writeFileSync(path.join(dir, 'index.js'), "console.log('ran:spaced');\n");
  const res = run(['--vfs-load', dir]);
  assert.strictEqual(res.status, 0, res.stderr);
  assert.match(res.stdout, /ran:spaced/);
}

// A source whose path holds spaces or quotes is mounted as given. Windows
// forbids `"` in a file name, so only the spaces and the `$` are exercised
// there.
{
  const oddName = common.isWindows ? `${id++}-od d $x` : `${id++}-od d "q" $x`;
  const dir = path.join(tmpdir.path, oddName);
  fs.mkdirSync(dir, { recursive: true });
  fs.writeFileSync(path.join(dir, 'index.js'), 'console.log("ran:odd");\n');
  const res = run([`--vfs-load=${dir}`]);
  assert.strictEqual(res.status, 0, res.stderr);
  assert.match(res.stdout, /ran:odd/);
}

// --vfs-load may only be given once: a second one would otherwise quietly
// replace the first.
{
  const dirs = {};
  for (const name of ['once-a', 'once-b']) {
    dirs[name] = fixture(name);
    fs.mkdirSync(dirs[name], { recursive: true });
    fs.writeFileSync(path.join(dirs[name], 'index.js'),
                     `console.log('ran:${name}');\n`);
  }

  const twice = run([`--vfs-load=${dirs['once-a']}`,
                     `--vfs-load=${dirs['once-b']}`]);
  assert.notStrictEqual(twice.status, 0);
  assert.match(twice.stderr, /--vfs-load may only be given once/);
}

// --vfs-load picks the entry point, so it is refused in NODE_OPTIONS: the
// environment must not be able to redirect what a `node <args>` run executes.
// Everything but the flag under test is passed on the command line, so a build
// that ignores NODE_OPTIONS cannot make this pass for the wrong reason.
if (hasNodeOptions) {
  const dir = fixture('env-refused');
  fs.mkdirSync(dir, { recursive: true });
  fs.writeFileSync(path.join(dir, 'index.js'), 'console.log("ran");\n');

  // On its own, and alongside a --vfs-load the command line legitimately gave:
  // the environment is refused either way rather than merged.
  for (const args of [['--experimental-vfs'],
                      ['--experimental-vfs', `--vfs-load=${dir}`]]) {
    const res = spawnSync(process.execPath, args, {
      encoding: 'utf8',
      env: { ...process.env, NODE_OPTIONS: envArg('--vfs-load', dir) },
    });
    assert.notStrictEqual(res.status, 0);
    assert.match(res.stderr, /--vfs-load.* is not allowed in NODE_OPTIONS/);
  }
}

// --experimental-vfs and --vfs-load may arrive from different places: the
// options are validated once every source has been parsed, so a --vfs-load on
// the command line is not rejected for an --experimental-vfs that only
// NODE_OPTIONS carries.
if (hasNodeOptions) {
  const dir = fixture('env-flag-cli-load');
  fs.mkdirSync(dir, { recursive: true });
  fs.writeFileSync(path.join(dir, 'index.js'), 'console.log("ran");\n');

  const res = spawnSync(process.execPath, [`--vfs-load=${dir}`], {
    encoding: 'utf8',
    env: { ...process.env, NODE_OPTIONS: '--experimental-vfs' },
  });
  assert.strictEqual(res.status, 0, res.stderr);
  assert.match(res.stdout, /ran/);
}

// Under --vfs-load the entry point comes from the mount, so no positional
// argument is consumed as one: every positional reaches the program verbatim
// from argv[2] onward, and argv[1] reports the mounted source.
{
  const dir = fixture('argv-app');
  fs.mkdirSync(dir, { recursive: true });
  fs.writeFileSync(path.join(dir, 'index.js'),
                   'console.log(JSON.stringify(process.argv.slice(1)));\n');

  for (const extra of [[], ['alpha'], ['alpha', 'beta']]) {
    const res = run([`--vfs-load=${dir}`, ...extra]);
    assert.strictEqual(res.status, 0, res.stderr);
    assert.deepStrictEqual(JSON.parse(res.stdout), [dir, ...extra]);
  }

  // A path-like argument must not be resolved against the real file system the
  // way a genuine entry-point argument would be.
  const res = run([`--vfs-load=${dir}`, './not/an/entry.js']);
  assert.strictEqual(res.status, 0, res.stderr);
  assert.deepStrictEqual(JSON.parse(res.stdout), [dir, './not/an/entry.js']);
}
