'use strict';
var cp = require('child_process');
var fs = require('fs');
var path = require('path');

var toolsPath = path.join(__dirname, '..', '..', 'deps', 'v8', 'tools');
var scriptFiles = [
    path.join(__dirname, 'polyfill.js'),
    path.join(toolsPath, 'splaytree.js'),
    path.join(toolsPath, 'codemap.js'),
    path.join(toolsPath, 'csvparser.js'),
    path.join(toolsPath, 'consarray.js'),
    path.join(toolsPath, 'csvparser.js'),
    path.join(toolsPath, 'consarray.js'),
    path.join(toolsPath, 'profile.js'),
    path.join(toolsPath, 'profile_view.js'),
    path.join(toolsPath, 'logreader.js'),
    path.join(toolsPath, 'tickprocessor.js'),
    path.join(toolsPath, 'SourceMap.js'),
    path.join(toolsPath, 'tickprocessor-driver.js')];
var tempScript = path.join(__dirname, 'tick-processor-tmp-' + process.pid);

process.on('exit', function() {
  try { fs.unlinkSync(tempScript); } catch (e) {}
});
process.on('uncaughtException', function(err) {
  try { fs.unlinkSync(tempScript); } catch (e) {}
  throw err;
});

var inStreams = scriptFiles.map(function(f) {
  return fs.createReadStream(f);
});
var outStream = fs.createWriteStream(tempScript);
inStreams.reduce(function(prev, curr, i) {
  prev.on('end', function() {
    curr.pipe(outStream, { end: i === inStreams.length - 1});
  });
  return curr;
});
inStreams[0].pipe(outStream, { end: false });
outStream.on('close', function() {
  var tickArguments = [tempScript];
  if (process.platform === 'darwin') {
    tickArguments.push('--mac', '--nm=' + path.join(toolsPath, 'mac-nm'));
  } else if (process.platform === 'win32') {
    tickArguments.push('--windows');
  }
  tickArguments.push.apply(tickArguments, process.argv.slice(2));
  var processTicks = cp.spawn(process.execPath, tickArguments, { stdio: 'inherit' });
});
