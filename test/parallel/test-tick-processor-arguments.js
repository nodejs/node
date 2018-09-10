'use strict';
require('../common');
const tmpdir = require('../common/tmpdir');
const fs = require('fs');
const assert = require('assert');
const { spawnSync } = require('child_process');

tmpdir.refresh();
process.chdir(tmpdir.path);

// Generate log file.
spawnSync(process.execPath, [ '--prof', '-p', '42' ]);

const logfile = fs.readdirSync('.').filter((name) => name.endsWith('.log'))[0];
assert(logfile);

// Make sure that the --preprocess argument is passed through correctly.
const { stdout } = spawnSync(
  process.execPath,
  [ '--prof-process', '--preprocess', logfile ],
  { encoding: 'utf8' });

// Make sure that the result is valid JSON.
JSON.parse(stdout);
