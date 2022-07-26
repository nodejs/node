#!/usr/bin/env node
// Usage: tools/update-timezone.mjs [--dry]
// Passing --dry will redirect output to stdout.
import { execSync, spawnSync } from 'node:child_process';
import { renameSync } from 'node:fs';
import { exit } from 'node:process';

const fileNames = [
  'zoneinfo64.res',
  'windowsZones.res',
  'timezoneTypes.res',
  'metaZones.res',
];
const dirs = spawnSync(
  'ls', {
    cwd: 'icu-data/tzdata/icunew',
    stdio: ['inherit', 'pipe', 'inherit']
  }
);

const currentVersion = process.versions.tz;
const availableVersions = dirs.stdout
      .toString()
      .split('\n')
      .filter((_) => _);
const latestVersion = availableVersions.sort()[0];

if (latestVersion === currentVersion) {
  exit();
}

execSync('bzip2 -d deps/icu-small/source/data/in/icudt*.dat.bz2');
fileNames.forEach((file) => {
  renameSync(`icu-data/tzdata/icunew/${latestVersion}/44/le/${file}`, `deps/icu-small/source/data/in/${file}`);
  spawnSync(
    'icupkg', [
      '-a',
      file,
      'icudt*.dat',
    ], { cwd: 'deps/icu-small/source/data/in/' }
  );
  spawnSync(
    'rm', [
      file
    ], { cwd: 'deps/icu-small/source/data/in/' }
  )
});
execSync('bzip2 -z deps/icu-small/source/data/in/icudt*.dat');
execSync('rm -rf icu-data');
