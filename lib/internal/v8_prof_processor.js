'use strict';

const vm = require('vm');

const scriptFiles = [
  'internal/v8_prof_polyfill',
  'internal/deps/v8/tools/splaytree',
  'internal/deps/v8/tools/codemap',
  'internal/deps/v8/tools/csvparser',
  'internal/deps/v8/tools/consarray',
  'internal/deps/v8/tools/profile',
  'internal/deps/v8/tools/profile_view',
  'internal/deps/v8/tools/logreader',
  'internal/deps/v8/tools/arguments',
  'internal/deps/v8/tools/tickprocessor',
  'internal/deps/v8/tools/SourceMap',
  'internal/deps/v8/tools/tickprocessor-driver'
];
var script = '';

scriptFiles.forEach(function(s) {
  script += process.binding('natives')[s] + '\n';
});

const tickArguments = [];
if (process.platform === 'darwin') {
  tickArguments.push('--mac');
} else if (process.platform === 'win32') {
  tickArguments.push('--windows');
}
tickArguments.push.apply(tickArguments, process.argv.slice(1));
script = `(function(module, require) {
  arguments = ${JSON.stringify(tickArguments)};
  function write (s) { process.stdout.write(s) }
  function printErr(err) { console.error(err); }
  ${script}
})`;
vm.runInThisContext(script)(module, require);
