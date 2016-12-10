'use strict'
var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var osenv = require('osenv')
var requireInject = require('require-inject')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')

var pkg = path.resolve(__dirname, 'bitbucket-https-url-with-creds-package')

var json = {
  name: 'bitbucket-https-url-with-creds-package',
  version: '0.0.0',
  dependencies: {
    'private': 'git+https://user:pass@bitbucket.org/foo/private.git'
  }
}

test('setup', function (t) {
  setup()
  t.end()
})

test('bitbucket-https-url-with-creds-package', function (t) {
  var cloneUrls = [
    ['https://user:pass@bitbucket.org/foo/private.git', 'Bitbucket URLs with passwords try only that.']
  ]

  var npm = requireInject.installGlobally('../../lib/npm.js', {
    'child_process': {
      'execFile': function (cmd, args, options, cb) {
        process.nextTick(function () {
          if (args.indexOf('clone') === -1) return cb(null, '', '')
          var cloneUrl = cloneUrls.shift()
          if (cloneUrl) {
            t.is(args[args.length - 2], cloneUrl[0], cloneUrl[1])
          } else {
            t.fail('too many attempts to clone')
          }
          cb(new Error())
        })
      }
    }
  })

  var opts = {
    cache: path.resolve(pkg, 'cache'),
    prefix: pkg,
    registry: common.registry,
    loglevel: 'silent'
  }
  npm.load(opts, function (er) {
    t.ifError(er, 'npm loaded without error')
    npm.commands.install([], function (er) {
      t.ok(er, 'mocked install failed as expected')
      t.end()
    })
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function setup () {
  cleanup()
  mkdirp.sync(pkg)
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(json, null, 2)
  )
  process.chdir(pkg)
}

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
}
