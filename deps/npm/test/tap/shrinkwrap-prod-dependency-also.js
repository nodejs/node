var fs = require('fs')
var path = require('path')

var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')
var pkg = common.pkg
var opts = [
  '--cache=' + path.resolve(pkg, 'cache'),
  '--registry=' + common.registry
]

function reportOutput (t, fh, out) {
  var trimmed = out.trim()
  if (!trimmed.length) return
  var prefix = fh + '> '
  t.comment(prefix + trimmed.split(/\n/).join('\n' + prefix))
}

var server
test("shrinkwrap --also=development doesn't strip out prod dependencies", function (t) {
  t.plan(4)

  mr({port: common.port}, function (er, s) {
    server = s
    setup()
    common.npm(['install', '.'].concat(opts), {cwd: pkg}, function (err, code, stdout, stderr) {
      if (err) return t.fail(err)
      t.is(code, 0, 'install')
      reportOutput(t, 'out', stdout)
      reportOutput(t, 'err', stderr)
      common.npm(['shrinkwrap', '--also=development'].concat(opts), {cwd: pkg}, function (err, code, stdout, stderr) {
        if (err) return t.fail(err)
        var ok = t.is(code, 0, 'shrinkwrap')
        reportOutput(t, 'out', stdout)
        reportOutput(t, 'err', stderr)
        if (ok) {
          try {
            var results = JSON.parse(fs.readFileSync(path.join(pkg, 'npm-shrinkwrap.json')))
            t.pass('read shrinkwrap')
          } catch (ex) {
            t.ifError(ex, 'read shrinkwrap')
          }
        }
        t.deepEqual(
          results.dependencies,
          desired.dependencies,
          'results have dev dep'
        )
        s.done()
        t.end()
      })
    })
  })
})

test('cleanup', function (t) {
  server.close()
  cleanup()
  t.end()
})

var desired = {
  name: 'npm-test-shrinkwrap-prod-dependency',
  version: '0.0.0',
  dependencies: {
    request: {
      version: '0.9.0',
      resolved: common.registry + '/request/-/request-0.9.0.tgz',
      integrity: 'sha1-EEn1mm9GWI5tAwkh+7hMovDCcU4='
    },
    underscore: {
      dev: true,
      version: '1.5.1',
      resolved: common.registry + '/underscore/-/underscore-1.5.1.tgz',
      integrity: 'sha1-0r3oF9F2/63olKtxRY5oKhS4bck='
    }
  }
}

var json = {
  author: 'Domenic Denicola',
  name: 'npm-test-shrinkwrap-prod-dependency',
  version: '0.0.0',
  dependencies: {
    request: '0.9.0'
  },
  devDependencies: {
    underscore: '1.5.1'
  }
}

function setup (opts) {
  cleanup()
  mkdirp.sync(pkg)
  fs.writeFileSync(path.join(pkg, 'package.json'), JSON.stringify(json, null, 2))
}

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
}
