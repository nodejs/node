'use strict';
var common = require('../common');
var path = require('path');
var fs = require('fs');

var testDir = common.tmpDir;
var testsubdir = path.join(testDir, 'testsubdir');
var filepath = path.join(testsubdir, 'watch.txt');

function cleanup() {
  try { fs.unlinkSync(filepath); } catch (e) { }
  try { fs.rmdirSync(testsubdir); } catch (e) { }
}
process.on('exit', cleanup);
cleanup();

try { fs.mkdirSync(testsubdir, 0o700); } catch (e) {}

// Need a grace period, else the mkdirSync() above fires off an event.
setTimeout(function() {
  var watcher = fs.watch(testDir, { persistent: true }, common.fail);
  setTimeout(function() {
    fs.writeFileSync(filepath, 'test');
  }, 100);
  setTimeout(function() {
    watcher.close();
  }, 500);
}, 50);
