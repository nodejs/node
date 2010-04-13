require('../common');
var sys = require('sys'), i;

var N = 30;
var done = [];

function get_printer(timeout) {
  return function () {
    sys.puts("Running from setTimeout " + timeout);
    done.push(timeout);
  };
}

process.nextTick(function () {
  sys.puts("Running from nextTick");
  done.push('nextTick');
})

for (i = 0; i < N; i += 1) {
  setTimeout(get_printer(i), i);
}

sys.puts("Running from main.");


process.addListener('exit', function () {
  assert.equal('nextTick', done[0]);
  for (i = 0; i < N; i += 1) {
    assert.equal(i, done[i+1]);
  }
});
