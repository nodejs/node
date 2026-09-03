'use strict';

// Covers --vfs-mount / --vfs-load: running a mounted directory's entry point
// with require() resolving inside the mount, a provider registered by either a
// -r (CJS) or an --import (ESM) preload backing a non-directory source, a ZIP
// archive claimed by the built-in provider, a worker inheriting the mounts,
// and the position of --vfs-load among the mounts deciding which one runs.
//
// Native addon loading from a mount is not exercised here (it needs a compiled
// .node), only the startup wiring around it.

require('../common');
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

// Two different ZIP archives mounted together each keep their own contents.
// The built-in provider opens the archive while deciding whether it can claim
// the source and hands that same handle to the provider it then creates, so
// this pins down that the handle belongs to the source it was opened for and
// is not shared between mounts.
{
  const zlib = require('zlib');

  // Each archive prints which one it is and what it can see, so a mix-up shows
  // up as the wrong marker or the other archive's file.
  const body = Buffer.from(
    'const fs = require("fs");\n' +
    'console.log("marker:" + fs.readFileSync(__dirname + "/marker.txt", "utf8").trim());\n' +
    'console.log("entries:" + fs.readdirSync(__dirname).sort().join(","));\n');

  function archive(name, unique) {
    const zipPath = fixture(`${name}.zip`);
    const entries = [
      zlib.ZipEntry.createSync('index.js', body),
      zlib.ZipEntry.createSync('marker.txt', Buffer.from(`${name}\n`)),
      zlib.ZipEntry.createSync(unique, Buffer.from('x\n')),
    ];
    const chunks = [];
    for (const chunk of zlib.createZipArchiveSync(entries)) chunks.push(chunk);
    fs.writeFileSync(zipPath, Buffer.concat(chunks));
    return zipPath;
  }

  const first = archive('first-archive', 'first-only.txt');
  const second = archive('second-archive', 'second-only.txt');

  // Whichever archive --vfs-load names is the one that runs, in either order,
  // and it sees its own entries rather than the other archive's.
  for (const [args, name, unique, absent] of [
    [[`--vfs-load=${first}`, `--vfs-mount=${second}`],
     'first-archive', 'first-only.txt', 'second-only.txt'],
    [[`--vfs-mount=${first}`, `--vfs-load=${second}`],
     'second-archive', 'second-only.txt', 'first-only.txt'],
    [[`--vfs-mount=${second}`, `--vfs-load=${first}`],
     'first-archive', 'first-only.txt', 'second-only.txt'],
  ]) {
    const res = run(args);
    assert.strictEqual(res.status, 0, res.stderr);
    assert.match(res.stdout, new RegExp(`marker:${name}`));
    assert.match(res.stdout, new RegExp(`entries:.*${unique}`));
    assert.doesNotMatch(res.stdout, new RegExp(absent));
  }
}

// A worker inherits --vfs-mount, so a worker script that lives inside the mount
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

// --vfs-load names the source it loads, so it always takes a value.
{
  const res = run(['--vfs-load']);
  assert.notStrictEqual(res.status, 0);
  assert.match(res.stderr, /--vfs-load requires an argument/);
}

// --vfs-mount and --vfs-load share one ordered list, so mounts happen in the
// order written and the entry point comes from whichever source --vfs-load
// names, wherever it sits among them.
{
  const dirs = {};
  for (const name of ['a', 'b', 'c']) {
    dirs[name] = fixture(name);
    fs.mkdirSync(dirs[name], { recursive: true });
    fs.writeFileSync(path.join(dirs[name], 'index.js'),
                     `console.log('ran:${name}');\n`);
  }

  for (const [args, expected] of [
    [[`--vfs-load=${dirs.a}`, `--vfs-mount=${dirs.b}`], 'a'],
    [[`--vfs-mount=${dirs.a}`, `--vfs-load=${dirs.b}`, `--vfs-mount=${dirs.c}`], 'b'],
    [[`--vfs-mount=${dirs.a}`, `--vfs-mount=${dirs.b}`, `--vfs-load=${dirs.c}`], 'c'],
    // The value may also be given as a separate argument.
    [['--vfs-mount', dirs.a, '--vfs-load', dirs.b], 'b'],
  ]) {
    const res = run(args);
    assert.strictEqual(res.status, 0, res.stderr);
    assert.match(res.stdout, new RegExp(`ran:${expected}`));
  }
}

// The same source given twice is mounted twice, at two mount points. The entry
// point comes from the one --vfs-load contributed, not from the earlier mount
// of the same source.
{
  const dir = fixture('twice');
  fs.mkdirSync(dir, { recursive: true });
  fs.writeFileSync(path.join(dir, 'index.js'),
                   'console.log("dir:" + __dirname);\n');

  const res = run([`--vfs-mount=${dir}`, `--vfs-load=${dir}`]);
  assert.strictEqual(res.status, 0, res.stderr);
  const [, first] = /dir:(\S+)/.exec(res.stdout);

  // With the order reversed the entry point is the other mount point, which is
  // what shows that the position decides and not the source.
  const reversed = run([`--vfs-load=${dir}`, `--vfs-mount=${dir}`]);
  assert.strictEqual(reversed.status, 0, reversed.stderr);
  const [, second] = /dir:(\S+)/.exec(reversed.stdout);
  assert.notStrictEqual(first, second);
}

// --vfs-load may only be given once: it shares one list with --vfs-mount, so a
// second one would otherwise quietly win over the first.
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

  // Repeating --vfs-mount stays allowed; only the loading one is limited.
  const many = run([`--vfs-mount=${dirs['once-a']}`,
                    `--vfs-load=${dirs['once-b']}`,
                    `--vfs-mount=${dirs['once-a']}`]);
  assert.strictEqual(many.status, 0, many.stderr);
  assert.match(many.stdout, /ran:once-b/);
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

  // --vfs-mount, by contrast, is accepted from the environment.
  const mountFromEnv = spawnSync(
    process.execPath, ['--experimental-vfs', `--vfs-load=${dir}`], {
      encoding: 'utf8',
      env: { ...process.env, NODE_OPTIONS: envArg('--vfs-mount', dir) },
    });
  assert.strictEqual(mountFromEnv.status, 0, mountFromEnv.stderr);
}

// --experimental-vfs and --vfs-mount may arrive from different places. The
// options are validated once every source has been parsed, so a mount from
// NODE_OPTIONS is not rejected for an --experimental-vfs that only the command
// line carries.
if (hasNodeOptions) {
  const dir = fixture('env-mount-cli-flag');
  fs.mkdirSync(dir, { recursive: true });
  fs.writeFileSync(path.join(dir, 'index.js'), 'console.log("ran");\n');

  const res = spawnSync(
    process.execPath, ['--experimental-vfs', `--vfs-load=${dir}`],
    { encoding: 'utf8',
      env: { ...process.env, NODE_OPTIONS: envArg('--vfs-mount', dir) } });
  assert.strictEqual(res.status, 0, res.stderr);
  assert.match(res.stdout, /ran/);
}

// --vfs-mount is allowed in NODE_OPTIONS and adds to the same ordered list.
// Because --vfs-load names its source rather than counting a position, it no
// longer matters that the environment is parsed first: what the command line
// loads is unaffected by how many mounts the environment contributed.
if (hasNodeOptions) {
  const dirs = {};
  for (const name of ['envA', 'cliX']) {
    dirs[name] = fixture(name);
    fs.mkdirSync(dirs[name], { recursive: true });
    fs.writeFileSync(path.join(dirs[name], 'index.js'),
                     `console.log('ran:${name}');\n`);
  }

  const res = spawnSync(
    process.execPath, ['--experimental-vfs', `--vfs-load=${dirs.cliX}`], {
      encoding: 'utf8',
      env: { ...process.env, NODE_OPTIONS: envArg('--vfs-mount', dirs.envA) },
    });
  assert.strictEqual(res.status, 0, res.stderr);
  assert.match(res.stdout, /ran:cliX/);
}

// A mount source holding spaces or quotes survives NODE_OPTIONS when quoted,
// which is the only way such a path can be expressed there at all.
if (hasNodeOptions) {
  const dir = path.join(tmpdir.path, `${id++}-od d "q" $x`);
  fs.mkdirSync(dir, { recursive: true });
  fs.writeFileSync(path.join(dir, 'index.js'), 'console.log("ran:odd");\n');

  const res = spawnSync(
    process.execPath, ['--experimental-vfs', `--vfs-load=${dir}`], {
      encoding: 'utf8',
      env: { ...process.env, NODE_OPTIONS: envArg('--vfs-mount', dir) },
    });
  assert.strictEqual(res.status, 0, res.stderr);
  assert.match(res.stdout, /ran:odd/);
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
