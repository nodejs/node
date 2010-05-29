require('../common');

path = require('path');
fs = require('fs');
fn = path.join(fixturesDir, 'elipses.txt');

var s = fs.readFileSync(fn, 'utf8');
for (var i = 0; i < s.length; i++) {
  assert.equal("\u2026", s[i]);
}
assert.equal(10000, s.length);
