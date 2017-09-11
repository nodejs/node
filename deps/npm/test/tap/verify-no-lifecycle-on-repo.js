'use strict'
var path = require('path')
var fs = require('graceful-fs')
var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var test = require('tap').test
var requireInject = require('require-inject')
require('../common-tap.js')

var base = path.join(__dirname, path.basename(__filename, '.js'))

var baseJSON = {
  name: 'base',
  version: '1.0.0',
  repository: {
    type: 'git',
    url: 'http://example.com'
  },
  scripts: {
    prepublish: 'false'
  }
}

var lastOpened
var npm = requireInject.installGlobally('../../lib/npm.js', {
  '../../lib/utils/lifecycle.js': function (pkg, stage, wd, moreOpts, cb) {
    if (typeof moreOpts === 'function') {
      cb = moreOpts
    }

    cb(new Error("Shouldn't be calling lifecycle scripts"))
  },
  opener: function (url, options, cb) {
    lastOpened = {url: url, options: options}
    cb()
  }
})

test('setup', function (t) {
  cleanup()
  setup()
  t.end()
})

test('repo', function (t) {
  process.chdir(base)
  npm.load({browser: 'echo'}, function () {
    npm.commands.repo([], function (err) {
      t.ifError(err, 'no errors')
      t.match(lastOpened.url, baseJSON.repository.url, 'opened the right url')
      t.is(lastOpened.options.command, 'echo', 'opened with a specified browser')
      t.end()
    })
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function saveJson (pkgPath, json) {
  mkdirp.sync(pkgPath)
  fs.writeFileSync(path.join(pkgPath, 'package.json'), JSON.stringify(json, null, 2))
}

function setup () {
  saveJson(base, baseJSON)
}

function cleanup () {
  rimraf.sync(base)
}
