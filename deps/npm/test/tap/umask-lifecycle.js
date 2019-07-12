var fs = require('fs')

var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var test = require('tap').test
var sprintf = require('sprintf-js').sprintf

var escapeExecPath = require('../../lib/utils/escape-exec-path.js')
var escapeArg = require('../../lib/utils/escape-arg.js')
var common = require('../common-tap.js')
var pkg = common.pkg

var nodeCmd = escapeExecPath(common.nodeBin)
var npmCmd = nodeCmd + ' ' + escapeArg(common.bin)
var umaskScript = npmCmd + ' config get umask && ' + nodeCmd + ' -pe "[process.env.npm_config_umask, process.umask()]"'

var pj = JSON.stringify({
  name: 'x',
  version: '1.2.3',
  scripts: { umask: umaskScript }
}, null, 2) + '\n'

var umask = process.umask()
var expected = [
  '',
  '> x@1.2.3 umask ' + pkg,
  '> ' + umaskScript,
  '',
  sprintf('%04o', umask),
  "[ '" + sprintf('%04o', umask) + "', " +
    sprintf('%d', umask) + ' ]',
  ''
].join('\n')

test('setup', function (t) {
  rimraf.sync(pkg)
  mkdirp.sync(pkg)
  fs.writeFileSync(pkg + '/package.json', pj)
  t.end()
})

test('umask script', function (t) {
  common.npm(['run', 'umask'], {
    cwd: pkg,
    env: {
      PATH: process.env.PATH,
      Path: process.env.Path,
      'npm_config_loglevel': 'warn'
    }
  }, function (er, code, sout, serr) {
    t.equal(sout, expected)
    t.equal(serr, '')
    t.end()
  })
})

test('clean', function (t) {
  rimraf.sync(pkg)
  t.end()
})
