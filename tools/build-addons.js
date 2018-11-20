'use strict';

// Usage: e.g. node build-addons.js <path to node-gyp> <directory>

const child_process = require('child_process');
const path = require('path');
const fs = require('fs').promises;
const util = require('util');

const execFile = util.promisify(child_process.execFile);

const parallelization = +process.env.JOBS || require('os').cpus().length;
const nodeGyp = process.argv[2];

async function runner(directoryQueue) {
  if (directoryQueue.length === 0)
    return;

  const dir = directoryQueue.shift();
  const next = () => runner(directoryQueue);

  try {
    // Only run for directories that have a `binding.gyp`.
    // (https://github.com/nodejs/node/issues/14843)
    await fs.stat(path.join(dir, 'binding.gyp'));
  } catch (err) {
    if (err.code === 'ENOENT' || err.code === 'ENOTDIR')
      return next();
    throw err;
  }

  console.log(`Building addon in ${dir}`);
  const { stdout, stderr } =
    await execFile(process.execPath, [nodeGyp, 'rebuild', `--directory=${dir}`],
                   {
                     stdio: 'inherit',
                     env: { ...process.env, MAKEFLAGS: '-j1' }
                   });

  // We buffer the output and print it out once the process is done in order
  // to avoid interleaved output from multiple builds running at once.
  process.stdout.write(stdout);
  process.stderr.write(stderr);

  return next();
}

async function main(directory) {
  const directoryQueue = (await fs.readdir(directory))
    .map((subdir) => path.join(directory, subdir));

  const runners = [];
  for (let i = 0; i < parallelization; ++i)
    runners.push(runner(directoryQueue));
  return Promise.all(runners);
}

main(process.argv[3]).catch((err) => setImmediate(() => { throw err; }));
