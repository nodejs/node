var test = require('tap').test
var npm = require('../../')

// This test has to be separate from `version-commit-hooks.js`, due to
// mutual exclusivity with the first test in that file. Initial configuration
// seems to only work as expected for defaults during the first `npm.load()`.

test('npm config `commit-hooks` defaults to `true`', function (t) {
  npm.load({}, function () {
    t.same(npm.config.get('commit-hooks'), true)
    t.end()
  })
})
