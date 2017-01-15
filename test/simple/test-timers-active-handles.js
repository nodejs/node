var cp = require('child_process');
var assert = require('assert');

// build deasync
cp.spawn(
    process.platform === 'win32' ? 'node-gyp.cmd' : 'node-gyp', ['rebuild'], {
      stdio: 'inherit',
      cwd: __dirname + '/test-timers-active-handles'
    })
  .on('exit', function (err) {
    if (err) {
      if (err === 127) {
        console.error(
          'node-gyp not found! Please upgrade your install of npm!'
        );
      } else {
        console.error('Build failed');
      }
      return process.exit(err);
    }
    test();
  });

function test() {
  var uvRunOnce = require('./test-timers-active-handles/build/Release/deasync');
  setTimeout(function () {
    var res;
    setTimeout(function () {
      res = true;
    }, 2);
    while (!res) {
      uvRunOnce.run();
    }
    assert.equal(res, true);
  }, 2);
}