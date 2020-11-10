'use strict'
var fs = require('graceful-fs')
var path = require('path')

var requireInject = require('require-inject')
var test = require('tap').test

var common = require('../common-tap.js')

var pkg = common.pkg

var json = {
  name: 'gitlab-shortcut',
  version: '0.0.0'
}

test('gitlab-shortcut', function (t) {
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(json, null, 2)
  )
  process.chdir(pkg)
  var cloneUrls = [
    ['https://gitlab.com/foo/private.git', 'GitLab shortcuts try HTTPS URLs second'],
    ['ssh://git@gitlab.com/foo/private.git', 'GitLab shortcuts try SSH first']
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
          cb(new Error('execFile mock fails on purpose'))
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
    npm.commands.install(['gitlab:foo/private'], function (er, result) {
      t.ok(er, 'mocked install failed as expected')
      t.end()
    })
  })
})
