'use strict';
const common = require('../common');
const tmpdir = require('../common/tmpdir');
const fs = require('fs');
const assert = require('assert');
const { spawnSyncAndAssert, spawnSyncAndExitWithoutError } = require('../common/child_process');

if (!common.enoughTestMem)
  common.skip('skipped due to memory requirements');
if (common.isAIX)
  common.skip('does not work on AIX');

tmpdir.refresh();

// Generate log file.
spawnSyncAndExitWithoutError(process.execPath, [ '--prof', '-p', '42' ], { cwd: tmpdir.path });

const files = fs.readdirSync(tmpdir.path);
const logfile = files.find((name) => /\.log$/.test(name));
assert(logfile);

// Drop the shared-library entries: the tick processor resolves the C++
// symbols of every listed library through nm (and c++filt on macOS), which is
// slow on builds that link many shared libraries and depends on the host
// toolchain. This test only checks that CLI arguments reach the tick
// processor; C++ symbol resolution is covered by test/tick-processor.
const logpath = tmpdir.resolve(logfile);
fs.writeFileSync(logpath, fs.readFileSync(logpath, 'utf8')
  .split('\n')
  .filter((line) => !line.startsWith('shared-library,'))
  .join('\n'));

// Make sure that the --preprocess argument is passed through correctly,
// as an example flag listed in deps/v8/tools/tickprocessor.js.
// Any of the other flags there should work for this test too, if --preprocess
// is ever removed.
spawnSyncAndAssert(
  process.execPath,
  [ '--prof-process', '--preprocess', logfile ],
  { cwd: tmpdir.path, encoding: 'utf8', maxBuffer: Infinity },
  {
    stdout(output) {
      // Make sure that the result is valid JSON.
      JSON.parse(output);
    }
  }
);
