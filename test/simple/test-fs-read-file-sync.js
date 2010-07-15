common = require("../common");
assert = common.assert

path = require('path');
fs = require('fs');
fn = path.join(common.fixturesDir, 'elipses.txt');

var s = fs.readFileSync(fn, 'utf8');
for (var i = 0; i < s.length; i++) {
  assert.equal("\u2026", s[i]);
}
assert.equal(10000, s.length);
