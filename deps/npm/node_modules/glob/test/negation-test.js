// Negation test
// Show that glob respect's minimatch's negate flag

var glob = require('../glob.js')
var test = require('tap').test

test('glob respects minimatch negate flag when activated with leading !', function(t) {
  var expect = ["abcdef/g", "abcfed/g", "c/d", "cb/e", "symlink/a"]
  var results = glob("!b**/*", {cwd: 'a'}, function (er, results) {
    if (er)
      throw er

    t.same(results, expect)
    t.end()
  });
});
