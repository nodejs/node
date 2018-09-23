var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')

var pkg = require('path').join(__dirname, 'ignore-shrinkwrap')

var EXEC_OPTS = { cwd: pkg }

var customMocks = {
  'get': {
    '/package.js': [200, { ente: true }],
    '/shrinkwrap.js': [200, { ente: true }]
  }
}

var json = {
  author: 'Rocko Artischocko',
  name: 'ignore-shrinkwrap',
  version: '0.0.0',
  dependencies: {
    'npm-test-ignore-shrinkwrap-file': 'http://localhost:1337/package.js'
  }
}

var shrinkwrap = {
  name: 'ignore-shrinkwrap',
  version: '0.0.0',
  dependencies: {
    'npm-test-ignore-shrinkwrap-file': {
      version: '1.2.3',
      from: 'http://localhost:1337/shrinkwrap.js',
      resolved: 'http://localhost:1337/shrinkwrap.js',
      dependencies: {
        opener: {
          version: '1.3.0',
          from: 'opener@1.3.0'
        }
      }
    }
  }
}

test('setup', function (t) {
  setup()
  t.end()
})

test('npm install --no-shrinkwrap', function (t) {
  mr({ port: common.port, mocks: customMocks }, function (err, s) {
    t.ifError(err, 'mock registry bootstrapped without issue')
    s._server.on('request', function (req) {
      switch (req.url) {
        case '/shrinkwrap.js':
          t.fail('npm-shrinkwrap.json used instead of package.json')
          break
        case '/package.js':
          t.pass('package.json used')
      }
    })

    common.npm(
      [
        '--registry', common.registry,
        '--loglevel', 'silent',
        'install', '--no-shrinkwrap'
      ],
      EXEC_OPTS,
      function (err, code) {
        t.ifError(err, 'npm ran without issue')
        t.ok(code, "install isn't going to succeed")
        s.close()
        t.end()
      }
    )
  })
})

test('npm install (with shrinkwrap)', function (t) {
  mr({ port: common.port, mocks: customMocks }, function (err, s) {
    t.ifError(err, 'mock registry bootstrapped without issue')
    s._server.on('request', function (req) {
      switch (req.url) {
        case '/shrinkwrap.js':
          t.pass('shrinkwrap used')
          break
        case '/package.js':
          t.fail('shrinkwrap ignored')
      }
    })

    common.npm(
      [
        '--registry', common.registry,
        '--loglevel', 'silent',
        'install'
      ],
      EXEC_OPTS,
      function (err, code) {
        t.ifError(err, 'npm ran without issue')
        t.ok(code, "install isn't going to succeed")
        s.close()
        t.end()
      }
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

function setup () {
  cleanup()
  mkdirp.sync(pkg)
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(json, null, 2)
  )
  fs.writeFileSync(
    path.join(pkg, 'npm-shrinkwrap.json'),
    JSON.stringify(shrinkwrap, null, 2)
  )
  process.chdir(pkg)
}
