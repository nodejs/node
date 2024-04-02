'use strict';
const common = require('../common');
const tmpdir = require('../common/tmpdir');
const fs = require('fs');
const assert = require('assert');
const { spawnSync } = require('child_process');

if (!common.enoughTestMem)
  common.skip('skipped due to memory requirements');
if (common.isAIX)
  common.skip('does not work on AIX');

tmpdir.refresh();

// Generate log file.
spawnSync(process.execPath, [ '--prof', '-p', '42' ], { cwd: tmpdir.path });

const files = fs.readdirSync(tmpdir.path);
const logfile = files.filter((name) => /\.log$/.test(name))[0];
assert(logfile);

// Make sure that the --preprocess argument is passed through correctly,
// as an example flag listed in deps/v8/tools/tickprocessor.js.
// Any of the other flags there should work for this test too, if --preprocess
// is ever removed.
const { stdout } = spawnSync(
  process.execPath,
  [ '--prof-process', '--preprocess', logfile ],
  { cwd: tmpdir.path, encoding: 'utf8', maxBuffer: Infinity });

// Make sure that the result is valid JSON.
JSON.parse(stdout);
