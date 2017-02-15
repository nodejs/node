var fs = require('fs')
var path = require('path')

var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var test = require('tap').test
var common = require('../common-tap.js')

var pkg = path.resolve(__dirname, 'npm-global-edit')

var editorSrc = function () { /*
#!/usr/bin/env node
var fs = require('fs')
if (fs.existsSync(process.argv[2])) {
  console.log('success')
} else {
  console.log('error')
  process.exit(1)
}
*/ }.toString().split('\n').slice(1, -1).join('\n')
var editorPath = path.join(pkg, 'editor')

test('setup', function (t) {
  cleanup(function (er) {
    t.ifError(er, 'old directory removed')

    mkdirp(pkg, '0777', function (er) {
      fs.writeFileSync(editorPath, editorSrc)
      fs.chmodSync(editorPath, '0777')
      t.ifError(er, 'created package directory correctly')
      t.end()
    })
  })
})

test('saving configs', function (t) {
  var opts = {
    cwd: pkg,
    env: {
      PATH: process.env.PATH,
      // We rely on the cwd + relative path combo here because otherwise,
      // this test will break if there's spaces in the editorPath
      EDITOR: 'node editor'
    }
  }
  common.npm(
    [
      'config',
      '--prefix', pkg,
      '--global',
      'edit'
    ],
    opts,
    function (err, code, stdout, stderr) {
      t.ifError(err, 'command ran without issue')

      t.equal(stderr, '', 'got nothing on stderr')
      t.equal(code, 0, 'exit ok')
      t.equal(stdout, 'success\n', 'got success message')
      t.end()
    }
  )
})

test('cleanup', function (t) {
  cleanup(function (er) {
    t.ifError(er, 'test directory removed OK')
    t.end()
  })
})

function cleanup (cb) {
  rimraf(pkg, cb)
}
