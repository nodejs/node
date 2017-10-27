'use strict'
var fs = require('fs')
var path = require('path')
var test = require('tap').test
var Tacks = require('tacks')
var File = Tacks.File
var Dir = Tacks.Dir
var common = require('../common-tap.js')
var testdir = path.join(__dirname, path.basename(__filename, '.js'))

var fixture = new Tacks(
  Dir({
    node_modules: Dir({
      'yes': Dir({
        'package.json': File({
          _requested: {
            rawSpec: 'file:///mods/yes'
          },
          dependencies: {},
          bin: {
            'yes': 'yes.js'
          },
          name: 'yes',
          version: '1.0.0'
        }),
        'yes.js': File('while (true) { console.log("y") }')
      }),
      '.bin': Dir({
        // verbose, but needed for `read-cmd-shim` to properly identify which
        // package this belongs to
        'yes': File(
          '#!/bin/sh\n' +
          'basedir=$(dirname "$(echo "$0" | sed -e \'s,\\\\,/,g\')")\n' +
          '\n' +
          'case `uname` in\n' +
          '    *CYGWIN*) basedir=`cygpath -w "$basedir"`;;\n' +
          'esac\n' +
          '\n' +
          'if [ -x "$basedir/node" ]; then\n' +
          '  "$basedir/node"  "$basedir/../yes/yes.js" "$@"\n' +
          '  ret=$?\n' +
          'else\n' +
          '  node  "$basedir/../yes/yes.js" "$@"\n' +
          '  ret=$?\n' +
          'fi\n' +
          'exit $ret\n'),
        'yes.cmd': File(
          '@IF EXIST "%~dp0\node.exe" (\n' +
          '"%~dp0\\node.exe"  "%~dp0\\..\\yes\\yes.js" %*\n' +
          ') ELSE (\n' +
          '@SETLOCAL\n' +
          '@SET PATHEXT=%PATHEXT:;.JS;=;%\n' +
          'node  "%~dp0\..\yes\yes.js" %*')
      })
    }),
    'package.json': File({
      name: 'test',
      version: '1.0.0',
      devDependencies: {
        'yes': 'file:///mods/yes'
      }
    })
  })
)

function setup () {
  cleanup()
  fixture.create(testdir)
}

function cleanup () {
  fixture.remove(testdir)
}

test('setup', function (t) {
  setup()
  t.end()
})

function readdir (dir) {
  try {
    return fs.readdirSync(dir)
  } catch (ex) {
    if (ex.code === 'ENOENT') return []
    throw ex
  }
}

test('prune cycle in dev deps', function (t) {
  common.npm(['prune', '--production', '--json'], {cwd: testdir}, function (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'prune finished successfully')
    t.like(JSON.parse(stdout), {removed: [{name: 'yes'}]}, 'removed the right modules')
    var dirs = readdir(testdir + '/node_modules').sort()
    // bindirs are never removed, it's ok for them to remain after prune
    t.same(dirs, ['.bin'])
    t.end()
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})
