var t = require('tap')
var winRebase = require('../lib/win-rebase')

t.test('it replaces path to node bin', function (t) {
  var result = winRebase('C:\\Program Files\\nodejs\\node.exe', 'C:\\foo')
  t.equal(result, 'C:\\foo')
  t.done()
})

t.test('it does not replace path if it references an unknown bin', function (t) {
  var result = winRebase('C:\\Program Files\\nodejs\\banana', 'C:\\foo')
  t.equal(result, 'C:\\Program Files\\nodejs\\banana')
  t.done()
})

t.test('replaces node bin and leaves the script being executed', function (t) {
  var result = winRebase('C:\\Program Files\\nodejs\\node.exe foo.js', 'C:\\foo')
  t.equal(result, 'C:\\foo foo.js')
  t.done()
})

t.test('handles a quote', function (t) {
  var result = winRebase('"C:\\Program Files\\nodejs\\node.exe" "foo.js"', 'C:\\foo')
  t.equal(result, '"C:\\foo" "foo.js"')
  t.end()
})

t.test('handles many quotes', function (t) {
  var result = winRebase('""C:\\Program Files\\nodejs\\node.exe" "foo.js""', 'C:\\foo')
  t.equal(result, '""C:\\foo" "foo.js""')
  t.end()
})

t.test('handles npm invocations', function (t) {
  var result = winRebase('""npm" "install""',
                         'C:\\foo',
                         function() { return 'C:\\path-to-npm\\npm' })
  t.equal(result, '""C:\\foo "C:\\path-to-npm\\node_modules\\npm\\bin\\npm-cli.js"" "install""')
  t.end()
})
