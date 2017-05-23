var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')

var pkg = path.join(__dirname, 'uninstall-package')

var EXEC_OPTS = { cwd: pkg, stdio: [0, 'pipe', 2] }

var json = {
  name: 'uninstall-package',
  version: '0.0.0',
  dependencies: {
    underscore: '~1.3.1',
    request: '~0.9.0'
  }
}

test('setup', function (t) {
  cleanup()
  mkdirp.sync(pkg)
  process.chdir(pkg)
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(json, null, 2)
  )

  t.end()
})

test('returns a list of removed items', function (t) {
  mr({ port: common.port }, function (er, s) {
    common.npm(
      [
        '--registry', common.registry,
        '--loglevel', 'error',
        'install', '.'
      ],
      EXEC_OPTS,
      function (err, code, stdout, stderr) {
        if (err) throw err
        t.notOk(code, 'install ran without raising error code')
        common.npm(
          [
            '--registry', common.registry,
            '--loglevel', 'error',
            '--parseable',
            'uninstall', 'underscore', 'request', 'lala'
          ],
          EXEC_OPTS,
          function (err, code, stdout, stderr) {
            if (err) throw err
            t.notOk(code, 'uninstall ran without raising error code')
            t.has(stdout, /^remove\tunderscore\t1.3.3\t/m, 'underscore uninstalled')
            t.has(stdout, /^remove\trequest\t0.9.5\t/m, 'request uninstalled')

            s.close()
            t.end()
          }
        )
      }
    )
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
}
