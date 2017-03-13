'use strict';

const fs = require('fs');
const os = require('os');
const { spawn, spawnSync } = require('child_process');
const { resolve } = require('path');

const kTopLevelDirectory = resolve(__dirname, '..');
const kAddonsDirectory = resolve(kTopLevelDirectory, 'test/addons');
const kNapiAddonsDirectory = resolve(kTopLevelDirectory, 'test/addons-napi');

// Location where the headers are installed to.
const kIncludeDirectory = kAddonsDirectory;

const kPython = process.env.PYTHON || 'python';
const kNodeGyp =
    resolve(kTopLevelDirectory, 'deps/npm/node_modules/node-gyp/bin/node-gyp');

process.chdir(kTopLevelDirectory);

// Copy headers to `${kIncludeDirectory}/include`.  install.py preserves
// timestamps so this won't cause unnecessary rebuilds.
{
  const args = [ 'tools/install.py', 'install', kIncludeDirectory, '/' ];
  const env = Object.assign({}, process.env);
  env.HEADERS_ONLY = 'yes, please';  // Ask nicely.
  env.LOGLEVEL = 'WARNING';

  const options = { env, stdio: 'inherit' };
  const result = spawnSync(kPython, args, options);
  if (result.status !== 0) process.exit(1);
}

// Scrape embedded add-ons from doc/api/addons.md.
require('./doc/addon-verify.js');

// Regenerate build files and rebuild if necessary.
let failures = 0;
process.on('exit', () => process.exit(failures > 0));

const jobs = [];

// test/gc contains a single add-on to build.
{
  const path = resolve(kTopLevelDirectory, 'test/gc');
  exec(path, 'configure', () => exec(path, 'build'));
}

for (const directory of [kAddonsDirectory, kNapiAddonsDirectory]) {
  for (const basedir of fs.readdirSync(directory)) {
    const path = resolve(directory, basedir);
    const gypfile = resolve(path, 'binding.gyp');
    if (!fs.existsSync(gypfile)) continue;
    exec(path, 'configure', () => exec(path, 'build'));
  }
}

// FIXME(bnoordhuis) I would have liked to derive the desired level of
// parallelism from MAKEFLAGS but it's missing the actual -j<jobs> flag.
for (const _ of os.cpus()) next();  // eslint-disable-line no-unused-vars

function next() {
  const job = jobs.shift();
  if (job) job();
}

function exec(path, command, done) {
  jobs.push(() => {
    if (failures > 0) return;

    const args = [kNodeGyp,
                  '--loglevel=silent',
                  '--directory=' + path,
                  '--nodedir=' + kIncludeDirectory,
                  '--python=' + kPython,
                  command];

    const env = Object.assign({}, process.env);
    env.MAKEFLAGS = '-j1';

    const options = { env, stdio: 'inherit' };
    const proc = spawn(process.execPath, args, options);

    proc.on('exit', (exitCode) => {
      if (exitCode !== 0) ++failures;
      if (done) done();
      next();
    });

    console.log(command, path);
  });
}
