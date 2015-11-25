'use strict';
var cp = require('child_process');
var fs = require('fs');
var path = require('path');

var scriptFiles = [
  'internal/v8_prof_polyfill',
  'v8/tools/splaytree',
  'v8/tools/codemap',
  'v8/tools/csvparser',
  'v8/tools/consarray',
  'v8/tools/profile',
  'v8/tools/profile_view',
  'v8/tools/logreader',
  'v8/tools/tickprocessor',
  'v8/tools/SourceMap',
  'v8/tools/tickprocessor-driver'
];
var tempScript = 'tick-processor-tmp-' + process.pid;
var tempNm = 'mac-nm-' + process.pid;

process.on('exit', function() {
  try { fs.unlinkSync(tempScript); } catch (e) {}
  try { fs.unlinkSync(tempNm); } catch (e) {}
});
process.on('uncaughtException', function(err) {
  try { fs.unlinkSync(tempScript); } catch (e) {}
  try { fs.unlinkSync(tempNm); } catch (e) {}
  throw err;
});

scriptFiles.forEach(function(script) {
  fs.appendFileSync(tempScript, process.binding('natives')[script]);
});
var tickArguments = [tempScript];
if (process.platform === 'darwin') {
  fs.writeFileSync(tempNm, process.binding('natives')['v8/tools/mac-nm'],
    { mode: 0o555 });
  tickArguments.push('--mac', '--nm=' + path.join(process.cwd(), tempNm));
} else if (process.platform === 'win32') {
  tickArguments.push('--windows');
}
tickArguments.push.apply(tickArguments, process.argv.slice(1));
cp.spawn(process.execPath, tickArguments, { stdio: 'inherit' });
