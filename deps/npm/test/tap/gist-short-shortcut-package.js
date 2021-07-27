'use strict'
var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var requireInject = require('require-inject')
var test = require('tap').test

var common = require('../common-tap.js')

var pkg = common.pkg

var json = {
  name: 'gist-short-shortcut-package',
  version: '0.0.0',
  dependencies: {
    'private-gist': 'gist:deadbeef'
  }
}

test('setup', function (t) {
  setup()
  t.end()
})

test('gist-short-shortcut-package', function (t) {
  var cloneUrls = [
    ['git://gist.github.com/deadbeef.git', 'GitHub gist shortcuts try git URLs first'],
    ['https://gist.github.com/deadbeef.git', 'GitHub gist shortcuts try HTTPS URLs third'],
    ['ssh://git@gist.github.com/deadbeef.git', 'GitHub gist shortcuts try SSH second']
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
    cache: common.cache,
    prefix: pkg,
    registry: common.registry,
    loglevel: 'silent'
  }
  npm.load(opts, function (er) {
    t.ifError(er, 'npm loaded without error')
    npm.commands.install([], function (er, result) {
      t.ok(er, 'mocked install failed as expected')
      t.end()
    })
  })
})

function setup () {
  mkdirp.sync(pkg)
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(json, null, 2)
  )
  process.chdir(pkg)
}
