// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.


var fs = require('fs');
var path = require('path');
var common = require('../common.js');
var packageJson = '{"main": "index.js"}';

var tmpDirectory = path.join(__dirname, '..', 'tmp');
var benchmarkDirectory = path.join(tmpDirectory, 'nodejs-benchmark-module');

var bench = common.createBenchmark(main, {
  thousands: [50]
});

function main(conf) {
  rmrf(tmpDirectory);
  try { fs.mkdirSync(tmpDirectory); } catch (e) {}
  try { fs.mkdirSync(benchmarkDirectory); } catch (e) {}

  var n = +conf.thousands * 1e3;
  for (var i = 0; i <= n; i++) {
    fs.mkdirSync(benchmarkDirectory + i);
    fs.writeFileSync(benchmarkDirectory + i + '/package.json', '{"main": "index.js"}');
    fs.writeFileSync(benchmarkDirectory + i + '/index.js', 'module.exports = "";');
  }

  measure(n);
}

function measure(n) {
  bench.start();
  for (var i = 0; i <= n; i++) {
    require(benchmarkDirectory + i);
  }
  bench.end(n / 1e3);
}

function rmrf(location) {
  if (fs.existsSync(location)) {
    var things = fs.readdirSync(location);
    things.forEach(function(thing) {
      var cur = path.join(location, thing),
          isDirectory = fs.statSync(cur).isDirectory();
      if (isDirectory) {
        rmrf(cur);
        return;
      }
      fs.unlinkSync(cur);
    });
    fs.rmdirSync(location);
  }
}
