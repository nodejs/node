var common = require('../common-tap.js')
var test = require('tap').test
var npm = require('../../lib/npm.js')
var mkdirp = require('mkdirp')
var path = require('path')
var fs = require('fs')
var mr = require('npm-registry-mock')

var PKG_DIR = common.pkg
let cacheIteration = 0
let CACHE_DIR

var DEFAULT_PKG = {
  'name': 'examples',
  'version': '1.2.3'
}

const isRoot = process.getuid && process.getuid() === 0
const sudoUID = isRoot ? +process.env.SUDO_UID : null
const sudoGID = isRoot ? +process.env.SUDO_GID : null
const { chownSync } = require('fs')
function resetPackage (options) {
  CACHE_DIR = path.resolve(common.cache, '' + cacheIteration++)
  mkdirp.sync(CACHE_DIR)
  npm.config.set('cache', CACHE_DIR)

  if (isRoot && sudoUID && sudoGID) {
    chownSync(CACHE_DIR, sudoUID, sudoGID)
  }

  fs.writeFileSync(path.resolve(PKG_DIR, 'package.json'), DEFAULT_PKG)
}

test('setup', function (t) {
  mkdirp.sync(PKG_DIR)
  process.chdir(PKG_DIR)

  mr({ port: common.port }, function (er, server) {
    if (er) {
      throw er
    }
    t.parent.teardown(() => server.close())
    npm.load({
      cache: common.cache,
      registry: common.registry,
      cwd: PKG_DIR
    }, function (err) {
      if (err) {
        throw err
      }
      t.end()
    })
  })
})

test('attempt publish with semver-like version', function (t) {
  resetPackage({})

  npm.config.set('tag', 'v1.x')
  npm.commands.publish([], function (err) {
    t.notEqual(err, null)
    t.same(err.message, 'Tag name must not be a valid SemVer range: v1.x')
    t.end()
  })
})

test('attempt publish with semver-like version', function (t) {
  resetPackage({})

  npm.config.set('tag', '1.2.3')
  npm.commands.publish([], function (err) {
    t.notEqual(err, null)
    t.same(err.message, 'Tag name must not be a valid SemVer range: 1.2.3')
    t.end()
  })
})
