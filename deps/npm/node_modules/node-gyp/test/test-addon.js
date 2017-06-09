'use strict'

var test = require('tape')
var execFile = require('child_process').execFile
var path = require('path')
var addonPath = path.resolve(__dirname, 'node_modules', 'hello_world')
var nodeGyp = path.resolve(__dirname, '..', 'bin', 'node-gyp.js')

test('build simple addon', function (t) {
  t.plan(3)

  // Set the loglevel otherwise the output disappears when run via 'npm test'
  var cmd = [nodeGyp, 'rebuild', '-C', addonPath, '--loglevel=verbose']
  var proc = execFile(process.execPath, cmd, function (err, stdout, stderr) {
    var logLines = stderr.toString().trim().split(/\r?\n/)
    var lastLine = logLines[logLines.length-1]
    t.strictEqual(err, null)
    t.strictEqual(lastLine, 'gyp info ok', 'should end in ok')
    try {
      var binding = require('hello_world')
      t.strictEqual(binding.hello(), 'world')
    } catch (error) {
      t.error(error, 'load module')
    }
  })
  proc.stdout.setEncoding('utf-8')
  proc.stderr.setEncoding('utf-8')
})
