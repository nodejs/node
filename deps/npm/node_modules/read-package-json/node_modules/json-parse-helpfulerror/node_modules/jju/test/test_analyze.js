var _assert = require('assert')
var analyze = require('../').analyze

function addTest(a, b) {
  if (typeof(describe) === 'function') {
    it('test_analyze: ' + a + ' == ' + b, function() {
      _assert.equal(a, b)
    })
  } else {
    _assert.equal(a, b)
  }
}

var t = analyze(JSON.stringify(require('os').networkInterfaces()))
addTest(t.has_whitespace, false)
addTest(t.has_comments, false)
addTest(t.has_newlines, false)
addTest(t.newline, '\n')
addTest(t.quote, '"')
addTest(t.quote_keys, true)
addTest(t.indent, '')

var t = analyze(JSON.stringify(require('os').networkInterfaces(), null, 2))
addTest(t.has_whitespace, true)
addTest(t.has_comments, false)
addTest(t.has_newlines, true)
addTest(t.newline, '\n')
addTest(t.quote, '"')
addTest(t.quote_keys, true)
addTest(t.indent, '  ')

var t = analyze(JSON.stringify(require('os').networkInterfaces(), null, 3))
addTest(t.indent, '   ')

var t = analyze(JSON.stringify(require('os').networkInterfaces(), null, '\t'))
addTest(t.indent, '\t')

var t = analyze(JSON.stringify(require('os').networkInterfaces(), null, 3).replace(/\n/g, '\r\n'))
addTest(t.indent, '   ')
addTest(t.newline, '\r\n')

var t = analyze(JSON.stringify(require('os').networkInterfaces()).replace(/"/g, "'"))
addTest(t.quote, "'")
addTest(t.quote_keys, true)

var t = analyze("{foo:'bar', 'bar':\"baz\", 'baz':\"quux\"}")
addTest(t.quote, "'")
addTest(t.quote_keys, false)

var t = analyze("{foo:'bar', \"bar\":'baz', \"baz\":\"quux\"}")
addTest(t.quote, '"')
addTest(t.quote_keys, false)

