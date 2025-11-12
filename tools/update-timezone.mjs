#!/usr/bin/env node
// Usage: tools/update-timezone.mjs
import { execSync } from 'node:child_process';
import { renameSync, readdirSync, rmSync } from 'node:fs';

const fileNames = [
  'zoneinfo64.res',
  'windowsZones.res',
  'timezoneTypes.res',
  'metaZones.res',
];

const availableVersions = readdirSync('icu-data/tzdata/icunew', { withFileTypes: true })
.filter((dirent) => dirent.isDirectory())
.map((dirent) => dirent.name);

const latestVersion = availableVersions.sort().at(-1);

execSync('bzip2 -d deps/icu-small/source/data/in/icudt*.dat.bz2');
fileNames.forEach((file) => {
  renameSync(`icu-data/tzdata/icunew/${latestVersion}/44/le/${file}`, `deps/icu-small/source/data/in/${file}`);
  execSync(`icupkg -a ${file} icudt*.dat`, { cwd: 'deps/icu-small/source/data/in/' });
  rmSync(`deps/icu-small/source/data/in/${file}`);
});
execSync('bzip2 -z deps/icu-small/source/data/in/icudt*.dat');
rmSync('icu-data', { recursive: true });
