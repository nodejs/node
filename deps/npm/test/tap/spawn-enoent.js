var path = require('path')
var test = require('tap').test
var fs = require('fs')
var rimraf = require('rimraf')
var mkdirp = require('mkdirp')
var common = require('../common-tap.js')

var pkg = path.resolve(__dirname, 'spawn-enoent')
var pj = JSON.stringify({
  name: 'x',
  version: '1.2.3',
  scripts: { start: 'wharble-garble-blorst' }
}, null, 2) + '\n'

test('setup', function (t) {
  rimraf.sync(pkg)
  mkdirp.sync(pkg)
  fs.writeFileSync(pkg + '/package.json', pj)
  t.end()
})

test('enoent script', function (t) {
  common.npm(['start'], {
    cwd: pkg,
    env: {
      PATH: process.env.PATH,
      Path: process.env.Path,
      'npm_config_loglevel': 'warn'
    }
  }, function (er, code, sout, serr) {
    t.similar(serr, /npm ERR! Failed at the x@1\.2\.3 start script 'wharble-garble-blorst'\./)
    t.end()
  })
})

test('clean', function (t) {
  rimraf.sync(pkg)
  t.end()
})
