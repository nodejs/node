#!/usr/bin/env node
// Usage: tools/update-timezone.mjs [--dry]
// Passing --dry will redirect output to stdout.
import { execSync, spawnSync } from 'node:child_process';
import { renameSync } from 'node:fs';
const fileNames = [
  'zoneinfo64.res',
  'windowsZones.res',
  'timezoneTypes.res',
  'metaZones.res',
];

execSync('rm -rf icu-data');
execSync('git clone https://github.com/unicode-org/icu-data');
execSync('bzip2 -d deps/icu-small/source/data/in/icudt70l.dat.bz2');
fileNames.forEach((file)=>{
  renameSync(`icu-data/tzdata/icunew/2022a/44/le/${file}`,`deps/icu-small/source/data/in/${file}`)
})
fileNames.forEach((file) => {
  spawnSync(
    `icupkg`,[
      `-a`,
      file,
      `icudt70l.dat`
    ],{cwd:`deps/icu-small/source/data/in/`}
  );
});
execSync('bzip2 -z deps/icu-small/source/data/in/icudt70l.dat')
fileNames.forEach((file)=>{
  renameSync(`deps/icu-small/source/data/in/${file}`,`icu-data/tzdata/icunew/2022a/44/le/${file}`)
})
execSync('rm -rf icu-data');

