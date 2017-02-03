'use strict';
const common = require('../common');
const path = require('path');
const fs = require('fs');

const testDir = common.tmpDir;
const testsubdir = path.join(testDir, 'testsubdir');
const filepath = path.join(testsubdir, 'watch.txt');

function cleanup() {
  try { fs.unlinkSync(filepath); } catch (e) { }
  try { fs.rmdirSync(testsubdir); } catch (e) { }
}
process.on('exit', cleanup);
cleanup();

try { fs.mkdirSync(testsubdir, 0o700); } catch (e) {}

// Need a grace period, else the mkdirSync() above fires off an event.
setTimeout(function() {
  const watcher = fs.watch(testDir, { persistent: true }, common.mustNotCall());
  setTimeout(function() {
    fs.writeFileSync(filepath, 'test');
  }, 100);
  setTimeout(function() {
    watcher.close();
  }, 500);
}, 50);
