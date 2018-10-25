'use strict';
const common = require('../common');
const assert = require('assert');
const child_process = require('child_process');
const fs = require('fs');
const stream = require('stream');

if (!common.isLinux)
  common.skip('The fs watch limit is OS-dependent');
if (!common.enoughTestCpu)
  common.skip('This test is resource-intensive');

try {
  // Ensure inotify limit is low enough for the test to actually exercise the
  // limit with small enough resources.
  const limit = Number(
    fs.readFileSync('/proc/sys/fs/inotify/max_user_watches', 'utf8'));
  if (limit > 16384)
    common.skip('inotify limit is quite large');
} catch (e) {
  if (e.code === 'ENOENT')
    common.skip('the inotify /proc subsystem does not exist');
  // Fail on other errors.
  throw e;
}

const processes = [];
const gatherStderr = new stream.PassThrough();
gatherStderr.setEncoding('utf8');
gatherStderr.setMaxListeners(Infinity);

let finished = false;
function spawnProcesses() {
  for (let i = 0; i < 10; ++i) {
    const proc = child_process.spawn(
      process.execPath,
      [ '-e',
        `process.chdir(${JSON.stringify(__dirname)});
        for (const file of fs.readdirSync('.'))
          fs.watch(file, () => {});`
      ], { stdio: ['inherit', 'inherit', 'pipe'] });
    proc.stderr.pipe(gatherStderr);
    processes.push(proc);
  }

  setTimeout(() => {
    if (!finished && processes.length < 200)
      spawnProcesses();
  }, 100);
}

spawnProcesses();

let accumulated = '';
gatherStderr.on('data', common.mustCallAtLeast((chunk) => {
  accumulated += chunk;
  if (accumulated.includes('Error:') && !finished) {
    assert(
      accumulated.includes('ENOSPC: System limit for number ' +
                           'of file watchers reached') ||
      accumulated.includes('EMFILE: '),
      accumulated);
    console.log(`done after ${processes.length} processes, cleaning up`);
    finished = true;
    processes.forEach((proc) => proc.kill());
  }
}, 1));
