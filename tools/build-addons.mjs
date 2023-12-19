#!/usr/bin/env node

// Usage: e.g. node build-addons.mjs <path to node-gyp> <directory>

import child_process from 'node:child_process';
import path from 'node:path';
import fs from 'node:fs/promises';
import util from 'node:util';
import process from 'node:process';
import os from 'node:os';

const execFile = util.promisify(child_process.execFile);

const parallelization = +process.env.JOBS || os.availableParallelism();
const nodeGyp = process.argv[2];
const directory = process.argv[3];

async function buildAddon(dir) {
  try {
    // Only run for directories that have a `binding.gyp`.
    // (https://github.com/nodejs/node/issues/14843)
    await fs.stat(path.join(dir, 'binding.gyp'));
  } catch (err) {
    if (err.code === 'ENOENT' || err.code === 'ENOTDIR')
      return;
    throw err;
  }

  console.log(`Building addon in ${dir}`);
  const { stdout, stderr } =
    await execFile(process.execPath, [nodeGyp, 'rebuild', `--directory=${dir}`],
                   {
                     stdio: 'inherit',
                     env: { ...process.env, MAKEFLAGS: '-j1' },
                   });

  // We buffer the output and print it out once the process is done in order
  // to avoid interleaved output from multiple builds running at once.
  process.stdout.write(stdout);
  process.stderr.write(stderr);
}

async function parallel(jobQueue, limit) {
  const next = async () => {
    if (jobQueue.length === 0) {
      return;
    }
    const job = jobQueue.shift();
    await job();
    await next();
  };

  const workerCnt = Math.min(limit, jobQueue.length);
  await Promise.all(Array.from({ length: workerCnt }, next));
}

const jobs = [];
for await (const dirent of await fs.opendir(directory)) {
  if (dirent.isDirectory()) {
    jobs.push(() => buildAddon(path.join(directory, dirent.name)));
  } else if (dirent.isFile() && dirent.name === 'binding.gyp') {
    jobs.push(() => buildAddon(directory));
  }
}
await parallel(jobs, parallelization);
