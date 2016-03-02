// compare with "google-chrome deps/v8/benchmarks/run.html"
'use strict';
var fs = require('fs');
var path = require('path');
var vm = require('vm');

var dir = path.join(__dirname, '..', '..', 'deps', 'v8', 'benchmarks');

global.print = function(s) {
  if (s === '----') return;
  console.log('misc/v8_bench.js %s', s);
};

global.load = function(filename) {
  var source = fs.readFileSync(path.join(dir, filename), 'utf8');
  // deps/v8/benchmarks/regexp.js breaks console.log() because it clobbers
  // the RegExp global,  Restore the original when the script is done.
  var $RegExp = global.RegExp;
  vm.runInThisContext(source, { filename: filename });
  global.RegExp = $RegExp;
};

global.load('run.js');
