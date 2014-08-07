var test = require('tap').test
var glob = require("../glob.js")

// Patterns that cannot match anything
var patterns = [
  '# comment',
  ' ',
  '\n',
  'just doesnt happen to match anything so this is a control'
]

patterns.forEach(function (p) {
  test(JSON.stringify(p), function (t) {
    glob(p, function (e, f) {
      t.equal(e, null, 'no error')
      t.same(f, [], 'no returned values')
      t.end()
    })
  })
})
