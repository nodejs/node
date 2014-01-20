var common = require('../common-tap')
  , test = require('tap').test
  , path = require('path')
  , spawn = require('child_process').spawn
  , rimraf = require('rimraf')
  , mkdirp = require('mkdirp')
  , pkg = __dirname + '/startstop'
  , cache = pkg + '/cache'
  , tmp = pkg + '/tmp'
  , node = process.execPath
  , npm = path.resolve(__dirname, '../../cli.js')

function run (command, t, parse) {
  var c = ''
    , e = ''
    , node = process.execPath
    , child = spawn(node, [npm, command], {
      cwd: pkg
    })

    child.stderr.on('data', function (chunk) {
      e += chunk
    })

    child.stdout.on('data', function (chunk) {
      c += chunk
    })

    child.stdout.on('end', function () {
      if (e) {
        throw new Error('npm ' + command + ' stderr: ' + e.toString())
      }
      if (parse) {
        // custom parsing function
        c = parse(c)
        t.equal(c.actual, c.expected)
        t.end()
        return
      }

      c = c.trim().split('\n')
      c = c[c.length - 1]
      t.equal(c, command)
      t.end()
    })

}

function cleanup () {
  rimraf.sync(pkg + '/cache')
  rimraf.sync(pkg + '/tmp')
}

test('setup', function (t) {
  cleanup()
  mkdirp.sync(pkg + '/cache')
  mkdirp.sync(pkg + '/tmp')
  t.end()

})

test('npm start', function (t) {
  run('start', t)
})

test('npm stop', function (t) {
  run('stop', t)
})

test('npm restart', function (t) {
  run ('restart', t, function (output) {
    output = output.split('\n').filter(function (val) {
      return val.match(/^s/)
    })
    return {actual: output, expected: output}
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})
