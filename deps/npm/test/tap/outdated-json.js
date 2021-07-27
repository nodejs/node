var fs = require('graceful-fs')
var path = require('path')

var mr = require('npm-registry-mock')
var test = require('tap').test

var common = require('../common-tap.js')
var server

var pkg = common.pkg

var EXEC_OPTS = { cwd: pkg }

var json = {
  name: 'outdated-json',
  author: 'Rockbert',
  version: '0.0.0',
  dependencies: {
    underscore: '~1.3.1'
  },
  devDependencies: {
    request: '~0.9.0'
  }
}

var expected = {
  underscore: {
    current: '1.3.3',
    wanted: '1.3.3',
    latest: '1.5.1',
    location: 'node_modules' + path.sep + 'underscore'
  },
  request: {
    current: '0.9.5',
    wanted: '0.9.5',
    latest: '2.27.0',
    location: 'node_modules' + path.sep + 'request'
  }
}

test('setup', function (t) {
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(json, null, 2)
  )
  process.chdir(pkg)
  mr({ port: common.port }, function (er, s) {
    t.ifError(er, 'mock registry should never fail to start')
    server = s
    common.npm(
      [
        '--registry', common.registry,
        '--silent',
        'install'
      ],
      EXEC_OPTS,
      function (err, code) {
        t.ifError(err, 'npm install ran without issue')
        t.notOk(code, 'npm install ran without raising error code')

        t.end()
      }
    )
  })
})

test('it should log json data', function (t) {
  common.npm(
    [
      '--registry', common.registry,
      '--silent',
      '--json',
      'outdated'
    ],
    EXEC_OPTS,
    function (err, code, stdout) {
      t.ifError(err, 'npm outdated ran without error')
      t.is(code, 1, 'npm outdated exited with code 1')
      var out
      t.doesNotThrow(function () {
        out = JSON.parse(stdout)
      }, 'output correctly parsed as JSON')
      t.deepEqual(out, expected)

      t.end()
    }
  )
})

test('it should log json data even when the list is empty', function (t) {
  common.npm(
    [
      'rm',
      'request',
      'underscore'
    ],
    EXEC_OPTS,
    function (er, code, stdout) {
      t.ifError(er, 'run without error')
      t.is(code, 0, 'successful exit status')
      common.npm(
        [
          '--registry', common.registry,
          '--silent',
          '--json',
          'outdated'
        ],
        EXEC_OPTS,
        function (er, code, stdout) {
          t.ifError(er, 'run without error')
          t.is(code, 0, 'successful exit status')
          t.same(JSON.parse(stdout), {}, 'got an empty object printed')
          t.end()
        }
      )
    }
  )
})

test('cleanup', function (t) {
  server.close()
  t.end()
})
