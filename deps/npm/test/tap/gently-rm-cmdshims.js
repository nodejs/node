'use strict'
/* eslint-disable camelcase */
var path = require('path')
var fs = require('graceful-fs')
var test = require('tap').test
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var npm = require('../../lib/npm.js')

var work = path.join(__dirname, path.basename(__filename, '.js'))
var doremove = path.join(work, 'doremove')
var dontremove = path.join(work, 'dontremove')
var example_json = {
  name: 'example',
  version: '1.0.0',
  bin: {
    'example': 'example.js'
  }
}
var example_bin =
  '#!/usr/bin/env node\n' +
  'true\n'

// NOTE: if this were actually produced on windows it would be \ not / of
// course, buuut, path.resolve doesn't understand \ outside of windows =/
var do_example_cmd =
  '@IF EXIST "%~dp0\\node.exe" (\n' +
  '  "%~dp0\\node.exe"  "%~dp0\\../example/example.js" %*\n' +
  ') ELSE (\n' +
  '  @SETLOCAL\n' +
  '  @SET PATHEXT=%PATHEXT:;.JS;=;%\n' +
  '  node  "%~dp0\\../example/example.js" %*\n' +
  ')\n'

var do_example_cygwin =
  '#!/bin/sh\n' +
  'basedir=`dirname "$0"`\n' +
  '\n' +
  'case `uname` in\n' +
  '    *CYGWIN*) basedir=`cygpath -w "$basedir"`;;\n' +
  'esac\n' +
  '\n' +
  'if [ -x "$basedir/node" ]; then\n' +
  '  "$basedir/node"  "$basedir/../example/example.js" "$@"\n' +
  '  ret=$?\n' +
  'else\n' +
  '  node  "$basedir/../example/example.js" "$@"\n' +
  '  ret=$?\n' +
  'fi\n' +
  'exit $ret\n'

var dont_example_cmd =
  '@IF EXIST "%~dp0\\node.exe" (\n' +
  '  "%~dp0\\node.exe"  "%~dp0\\../example-other/example.js" %*\n' +
  ') ELSE (\n' +
  '  @SETLOCAL\n' +
  '  @SET PATHEXT=%PATHEXT:;.JS;=;%\n' +
  '  node  "%~dp0\\../example-other/example.js" %*\n' +
  ')\n'

var dont_example_cygwin =
  '#!/bin/sh\n' +
  'basedir=`dirname "$0"`\n' +
  '\n' +
  'case `uname` in\n' +
  '    *CYGWIN*) basedir=`cygpath -w "$basedir"`;;\n' +
  'esac\n' +
  '\n' +
  'if [ -x "$basedir/node" ]; then\n' +
  '  "$basedir/node"  "$basedir/../example-other/example.js" "$@"\n' +
  '  ret=$?\n' +
  'else\n' +
  '  node  "$basedir/../example-other/example.js" "$@"\n' +
  '  ret=$?\n' +
  'fi\n' +
  'exit $ret\n'

function cleanup () {
  rimraf.sync(work)
}

var doremove_module = path.join(doremove, 'node_modules', 'example')
var doremove_example_cmd = path.join(doremove, 'node_modules', '.bin', 'example.cmd')
var doremove_example_cygwin = path.join(doremove, 'node_modules', '.bin', 'example')
var dontremove_module = path.join(dontremove, 'node_modules', 'example')
var dontremove_example_cmd = path.join(dontremove, 'node_modules', '.bin', 'example.cmd')
var dontremove_example_cygwin = path.join(dontremove, 'node_modules', '.bin', 'example')

function setup () {
  mkdirp.sync(doremove_module)
  mkdirp.sync(path.join(doremove, 'node_modules', '.bin'))
  fs.writeFileSync(path.join(doremove, 'node_modules', 'example', 'package.json'), JSON.stringify(example_json))
  fs.writeFileSync(path.join(doremove, 'node_modules', 'example', 'example.js'), JSON.stringify(example_bin))
  fs.writeFileSync(doremove_example_cmd, do_example_cmd)
  fs.writeFileSync(doremove_example_cygwin, do_example_cygwin)

  mkdirp.sync(dontremove_module)
  mkdirp.sync(path.join(dontremove, 'node_modules', '.bin'))
  fs.writeFileSync(path.join(dontremove, 'node_modules', 'example', 'package.json'), JSON.stringify(example_json))
  fs.writeFileSync(path.join(dontremove, 'node_modules', 'example', 'example.js'), JSON.stringify(example_bin))
  fs.writeFileSync(dontremove_example_cmd, dont_example_cmd)
  fs.writeFileSync(dontremove_example_cygwin, dont_example_cygwin)
}

test('setup', function (t) {
  cleanup()
  setup()
  npm.load({}, function () {
    t.done()
  })
})

// Like slide.chain, but runs all commands even if they have errors, also
// throws away results.
function runAll (cmds, done) {
  runNext()
  function runNext () {
    if (cmds.length === 0) return done()
    var cmdline = cmds.shift()
    var cmd = cmdline.shift()
    cmdline.push(runNext)
    cmd.apply(null, cmdline)
  }
}

test('remove-cmd-shims', function (t) {
  t.plan(2)

  var gentlyRm = require('../../lib/utils/gently-rm.js')
  runAll([ [gentlyRm, doremove_example_cmd, true, doremove_module],
    [gentlyRm, doremove_example_cygwin, true, doremove_module] ],
  function () {
    fs.stat(doremove_example_cmd, function (er) {
      t.is(er && er.code, 'ENOENT', 'cmd-shim was removed')
    })
    fs.stat(doremove_example_cygwin, function (er) {
      t.is(er && er.code, 'ENOENT', 'cmd-shim cygwin script was removed')
    })
  })
})

test('dont-remove-cmd-shims', function (t) {
  t.plan(2)
  var gentlyRm = require('../../lib/utils/gently-rm.js')
  runAll([ [gentlyRm, dontremove_example_cmd, true, dontremove_module],
    [gentlyRm, dontremove_example_cygwin, true, dontremove_module] ],
  function () {
    fs.stat(dontremove_example_cmd, function (er) {
      t.is(er, null, 'cmd-shim was not removed')
    })
    fs.stat(dontremove_example_cygwin, function (er) {
      t.is(er, null, 'cmd-shim cygwin script was not removed')
    })
  })
})

test('cleanup', function (t) {
  cleanup()
  t.done()
})
