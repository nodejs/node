var fs = require('graceful-fs')
var path = require('path')
var existsSync = fs.existsSync || path.existsSync

var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')

var pkg = common.pkg

var EXEC_OPTS = { cwd: pkg, stdio: [0, 1, 2] }

var json = {
  name: 'install-windows-newlines',
  description: 'fixture',
  version: '0.0.0',
  dependencies: {
    'cli-dependency': 'file:cli-dependency'
  }
}

var dependency = {
  name: 'cli-dependency',
  description: 'fixture',
  version: '0.0.0',
  bin: {
    hashbang: './hashbang.js',
    nohashbang: './nohashbang.js'
  }
}

test('setup', function (t) {
  cleanup()
  mkdirp.sync(path.join(pkg, 'cli-dependency'))
  fs.writeFileSync(
    path.join(pkg, 'cli-dependency', 'package.json'),
    JSON.stringify(dependency, null, 2)
  )
  fs.writeFileSync(
    path.join(pkg, 'cli-dependency', 'hashbang.js'),
    '#!/usr/bin/env node\r\nconsole.log(\'Hello, world!\')\r\n'
  )
  fs.writeFileSync(
    path.join(pkg, 'cli-dependency', 'nohashbang.js'),
    '\'use strict\'\r\nconsole.log(\'Goodbye, world!\')\r\n'
  )

  mkdirp.sync(path.join(pkg, 'node_modules'))
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(json, null, 2)
  )

  return common.npm(['install'], EXEC_OPTS).spread((code) => {
    t.equal(code, 0, 'npm install did not raise error code')
    t.ok(
      existsSync(path.resolve(pkg, 'node_modules/.bin/hashbang')),
      'hashbang installed'
    )
    t.ok(
      existsSync(path.resolve(pkg, 'node_modules/.bin/nohashbang')),
      'nohashbang installed'
    )
    t.notOk(
      fs.readFileSync(
        path.resolve(pkg, 'node_modules/cli-dependency/hashbang.js'),
        'utf8'
      ).includes('node\r\n'),
      'hashbang dependency cli newlines converted'
    )
    t.ok(
      fs.readFileSync(
        path.resolve(pkg, 'node_modules/cli-dependency/nohashbang.js'),
        'utf8'
      ).includes('\r\n'),
      'nohashbang dependency cli newlines retained'
    )
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function cleanup () {
  rimraf.sync(pkg)
}
