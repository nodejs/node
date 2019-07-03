// verify that prepublishOnly runs _only_ on publish
var join = require('path').join

var mr = require('npm-registry-mock')
var test = require('tap').test
var Tacks = require('tacks')
var File = Tacks.File
var Dir = Tacks.Dir
var path = require('path')

var common = require('../common-tap')

var pkg = common.pkg
var cachedir = join(pkg, 'cache')
var tmpdir = join(pkg, 'tmp')

var env = {
  'npm_config_cache': cachedir,
  'npm_config_tmp': tmpdir,
  'npm_config_prefix': pkg,
  'npm_config_global': 'false'
}

for (var i in process.env) {
  if (!/^npm_config_/.test(i)) {
    env[i] = process.env[i]
  }
}

var server

var fixture = new Tacks(Dir({
  cache: Dir(),
  tmp: Dir(),
  '.npmrc': File([
    'progress=false',
    'registry=' + common.registry,
    '//localhost:1337/:username=username',
    '//localhost:1337/:_authToken=deadbeeffeed'
  ].join('\n') + '\n'),
  helper: Dir({
    'script.js': File([
      '#!/usr/bin/env node\n',
      'console.log("ok")\n'
    ].join('\n') + '\n'
    ),
    'package.json': File({
      name: 'helper',
      version: '6.6.6',
      bin: './script.js'
    })
  }),
  'package.json': File({
    name: 'npm-test-prepublish-only',
    version: '1.2.5',
    dependencies: {
      'helper': 'file:./helper'
    },
    scripts: {
      build: 'helper',
      prepublishOnly: 'node ' + path.resolve(__dirname, '../../') + ' run build'
    }
  })
}))

test('setup', function (t) {
  cleanup()
  fixture.create(pkg)
  mr({port: common.port, throwOnUnmatched: true}, function (err, s) {
    t.ifError(err, 'registry mocked successfully')
    server = s
    common.npm(
      [
        'install',
        '--loglevel', 'error',
        '--cache', cachedir,
        '--tmp', tmpdir
      ],
      {
        cwd: pkg,
        env: env
      },
      function (err, code, stdout, stderr) {
        t.equal(code, 0, 'install exited OK')
        t.ifErr(err, 'installed successfully')

        t.notOk(stderr, 'got stderr data:' + JSON.stringify('' + stderr))

        t.end()
      }
    )
  })
})

test('test', function (t) {
  server.filteringRequestBody(function () { return true })
    .put('/npm-test-prepublish-only', true)
    .reply(201, {ok: true})

  common.npm(
    [
      'publish',
      '--loglevel', 'warn'
    ],
    {
      cwd: pkg,
      env: env
    },
    function (err, code, stdout, stderr) {
      t.equal(code, 0, 'publish ran without error')
      t.ifErr(err, 'published successfully')

      t.notOk(stderr, 'got stderr data:' + JSON.stringify('' + stderr))
      var c = stdout.trim()
      var regex = new RegExp(
        '> npm-test-prepublish-only@1.2.5 prepublishOnly [^\\r\\n]+\\r?\\n' +
        '> .* run build\\r?\\n' +
        '\\r?\\n' +
        '\\r?\\n' +
        '> npm-test-prepublish-only@1.2.5 build [^\\r\\n]+\\r?\\n' +
        '> helper\\r?\\n' +
        '\\r?\\n' +
        'ok\\r?\\n' +
        '\\+ npm-test-prepublish-only@1.2.5', 'ig'
      )

      t.match(c, regex)
      t.end()
    }
  )
})

test('cleanup', function (t) {
  cleanup()
  server.close()
  t.pass('cleaned up')
  t.end()
})

function cleanup () {
  fixture.remove(pkg)
}
