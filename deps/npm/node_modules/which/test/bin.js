var t = require('tap')
var spawn = require('child_process').spawn
var node = process.execPath
var bin = require.resolve('../bin/which')

function which (args, extraPath, cb) {
  if (typeof extraPath === 'function')
    cb = extraPath, extraPath = null

  var options = {}
  if (extraPath) {
    var sep = process.platform === 'win32' ? ';' : ':'
    var p = process.env.PATH + sep + extraPath
    options.env = Object.keys(process.env).reduce(function (env, k) {
      if (!k.match(/^path$/i))
        env[k] = process.env[k]
      return env
    }, { PATH: p })
  }

  var out = ''
  var err = ''
  var child = spawn(node, [bin].concat(args), options)
  child.stdout.on('data', function (c) {
    out += c
  })
  child.stderr.on('data', function (c) {
    err += c
  })
  child.on('close', function (code, signal) {
    cb(code, signal, out.trim(), err.trim())
  })
}

t.test('finds node', function (t) {
  which('node', function (code, signal, out, err) {
    t.equal(signal, null)
    t.equal(code, 0)
    t.equal(err, '')
    t.match(out, /[\\\/]node(\.exe)?$/)
    t.end()
  })
})

t.test('does not find flergyderp', function (t) {
  which('flergyderp', function (code, signal, out, err) {
    t.equal(signal, null)
    t.equal(code, 1)
    t.equal(err, '')
    t.match(out, '')
    t.end()
  })
})

t.test('finds node and tap', function (t) {
  which(['node', 'tap'], function (code, signal, out, err) {
    t.equal(signal, null)
    t.equal(code, 0)
    t.equal(err, '')
    t.match(out.split(/\n/), [
      /[\\\/]node(\.exe)?$/,
      /[\\\/]tap(\.cmd)?$/
    ])
    t.end()
  })
})

t.test('finds node and tap, but not flergyderp', function (t) {
  which(['node', 'flergyderp', 'tap'], function (code, signal, out, err) {
    t.equal(signal, null)
    t.equal(code, 1)
    t.equal(err, '')
    t.match(out.split(/\n/), [
      /[\\\/]node(\.exe)?$/,
      /[\\\/]tap(\.cmd)?$/
    ])
    t.end()
  })
})

t.test('cli flags', function (t) {
  var p = require('path').dirname(bin)
  var cases = [ '-a', '-s', '-as', '-sa' ]
  t.plan(cases.length)
  cases.forEach(function (c) {
    t.test(c, function (t) {
      which(['which', c], p, function (code, signal, out, err) {
        t.equal(signal, null)
        t.equal(code, 0)
        t.equal(err, '')
        if (/s/.test(c))
          t.equal(out, '', 'should be silent')
        else if (/a/.test(c))
          t.ok(out.split(/\n/).length > 1, 'should have more than 1 result')
        t.end()
      })
    })
  })
})

t.test('shows usage', function (t) {
  which([], function (code, signal, out, err) {
    t.equal(signal, null)
    t.equal(code, 1)
    t.equal(err, 'usage: which [-as] program ...')
    t.equal(out, '')
    t.end()
  })
})

t.test('complains about unknown flag', function (t) {
  which(['node', '-sax'], function (code, signal, out, err) {
    t.equal(signal, null)
    t.equal(code, 1)
    t.equal(out, '')
    t.equal(err, 'which: illegal option -- x\nusage: which [-as] program ...')
    t.end()
  })
})
