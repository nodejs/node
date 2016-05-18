/* eslint-disable strict */
const scriptFiles = [
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
var script = '';

scriptFiles.forEach(function(s) {
  script += process.binding('natives')[s] + '\n';
});

var tickArguments = [];
if (process.platform === 'darwin') {
  const nm = 'foo() { nm "$@" | (c++filt -p -i || cat) }; foo $@';
  tickArguments.push('--mac', '--nm=' + nm);
} else if (process.platform === 'win32') {
  tickArguments.push('--windows');
}
tickArguments.push.apply(tickArguments, process.argv.slice(1));
script = `(function() {
  arguments = ${JSON.stringify(tickArguments)};
  ${script}
})()`;
eval(script);
