var sw = require('../')
var isWindows = require('../lib/is-windows.js')()
var winNoShebang = isWindows && 'no shebang execution on windows'
var winNoSig = isWindows && 'no signals get through cmd'

var onExit = require('signal-exit')
var cp = require('child_process')
var fixture = require.resolve('./fixtures/script.js')
var npmFixture = require.resolve('./fixtures/npm')
var fs = require('fs')
var path = require('path')

if (process.argv[2] === 'parent') {
  // hang up once
  process.once('SIGHUP', function onHup () {
    console.log('SIGHUP')
  })
  // handle sigints forever
  process.on('SIGINT', function onInt () {
    console.log('SIGINT')
  })
  onExit(function (code, signal) {
    console.log('EXIT %j', [code, signal])
  })
  var argv = process.argv.slice(3).map(function (arg) {
    if (arg === fixture) {
      return '{{FIXTURE}}'
    }
    return arg
  })
  console.log('WRAP %j', process.execArgv.concat(argv))
  sw.runMain()
  return
}

var t = require('tap')
var unwrap = sw([__filename, 'parent'])

var expect = 'WRAP ["{{FIXTURE}}","xyz"]\n' +
  '[]\n' +
  '["xyz"]\n' +
  'EXIT [0,null]\n'

// dummy for node v0.10
if (!cp.spawnSync) {
  cp.spawnSync = function () {
    return {
      status: 0,
      signal: null,
      stdout: expect
    }
  }
}

t.test('spawn execPath', function (t) {
  t.plan(4)

  t.test('basic', function (t) {
    var child = cp.spawn(process.execPath, [fixture, 'xyz'])

    var out = ''
    child.stdout.on('data', function (c) {
      out += c
    })
    child.on('close', function (code, signal) {
      t.equal(code, 0)
      t.equal(signal, null)
      t.equal(out, expect)
      t.end()
    })
  })

  t.test('basic sync', function (t) {
    var child = cp.spawnSync(process.execPath, [fixture, 'xyz'])

    t.equal(child.status, 0)
    t.equal(child.signal, null)
    t.equal(child.stdout.toString(), expect)
    t.end()
  })

  t.test('SIGINT', { skip: winNoSig }, function (t) {
    var child = cp.spawn(process.execPath, [fixture, 'xyz'])

    var out = ''
    child.stdout.on('data', function (c) {
      out += c
    })
    child.stdout.once('data', function () {
      child.kill('SIGINT')
    })
    child.stderr.on('data', function (t) {
      console.error(t)
    })
    child.on('close', function (code, signal) {
      t.equal(code, 0)
      t.equal(signal, null)
      t.equal(out, 'WRAP ["{{FIXTURE}}","xyz"]\n' +
        '[]\n' +
        '["xyz"]\n' +
        'SIGINT\n' +
        'EXIT [0,null]\n')
      t.end()
    })
  })

  t.test('SIGHUP', { skip: winNoSig }, function (t) {
    var child = cp.spawn(process.execPath, [fixture, 'xyz'])

    var out = ''
    child.stdout.on('data', function (c) {
      out += c
      child.kill('SIGHUP')
    })
    child.on('close', function (code, signal) {
      t.equal(signal, 'SIGHUP')
      t.equal(out, 'WRAP ["{{FIXTURE}}","xyz"]\n' +
        '[]\n' +
        '["xyz"]\n' +
        'SIGHUP\n' +
        'EXIT [null,"SIGHUP"]\n')
      t.end()
    })
  })
})

t.test('spawn node', function (t) {
  t.plan(4)

  t.test('basic', function (t) {
    var child = cp.spawn('node', [fixture, 'xyz'])

    var out = ''
    child.stdout.on('data', function (c) {
      out += c
    })
    child.on('close', function (code, signal) {
      t.equal(code, 0)
      t.equal(signal, null)
      t.equal(out, expect)
      t.end()
    })
  })

  t.test('basic sync', function (t) {
    var child = cp.spawnSync('node', [fixture, 'xyz'])

    t.equal(child.status, 0)
    t.equal(child.signal, null)
    t.equal(child.stdout.toString(), expect)
    t.end()
  })

  t.test('SIGINT', { skip: winNoSig }, function (t) {
    var child = cp.spawn('node', [fixture, 'xyz'])

    var out = ''
    child.stdout.on('data', function (c) {
      out += c
    })
    child.stdout.once('data', function () {
      child.kill('SIGINT')
    })
    child.stderr.on('data', function (t) {
      console.error(t)
    })
    child.on('close', function (code, signal) {
      t.equal(code, 0)
      t.equal(signal, null)
      t.equal(out, 'WRAP ["{{FIXTURE}}","xyz"]\n' +
        '[]\n' +
        '["xyz"]\n' +
        'SIGINT\n' +
        'EXIT [0,null]\n')
      t.end()
    })
  })

  t.test('SIGHUP', { skip: winNoSig }, function (t) {
    var child = cp.spawn('node', [fixture, 'xyz'])

    var out = ''
    child.stdout.on('data', function (c) {
      out += c
      child.kill('SIGHUP')
    })
    child.on('close', function (code, signal) {
      t.equal(signal, 'SIGHUP')
      t.equal(out, 'WRAP ["{{FIXTURE}}","xyz"]\n' +
        '[]\n' +
        '["xyz"]\n' +
        'SIGHUP\n' +
        'EXIT [null,"SIGHUP"]\n')
      t.end()
    })
  })
})

t.test('exec execPath', function (t) {
  t.plan(4)

  t.test('basic', function (t) {
    var opt = isWindows ? null : { shell: '/bin/bash' }
    var child = cp.exec(process.execPath + ' ' + fixture + ' xyz', opt)

    var out = ''
    child.stdout.on('data', function (c) {
      out += c
    })
    child.on('close', function (code, signal) {
      t.equal(code, 0)
      t.equal(signal, null)
      t.equal(out, expect)
      t.end()
    })
  })

  t.test('execPath wrapped with quotes', function (t) {
    var opt = isWindows ? null : { shell: '/bin/bash' }
    var child = cp.exec(JSON.stringify(process.execPath) + ' ' + fixture +
      ' xyz', opt)

    var out = ''
    child.stdout.on('data', function (c) {
      out += c
    })
    child.on('close', function (code, signal) {
      t.equal(code, 0)
      t.equal(signal, null)
      t.equal(out, expect)
      t.end()
    })
  })

  t.test('SIGINT', { skip: winNoSig }, function (t) {
    var child = cp.exec(process.execPath + ' ' + fixture + ' xyz', { shell: '/bin/bash' })

    var out = ''
    child.stdout.on('data', function (c) {
      out += c
    })
    child.stdout.once('data', function () {
      child.kill('SIGINT')
    })
    child.stderr.on('data', function (t) {
      console.error(t)
    })
    child.on('close', function (code, signal) {
      t.equal(code, 0)
      t.equal(signal, null)
      t.equal(out, 'WRAP ["{{FIXTURE}}","xyz"]\n' +
        '[]\n' +
        '["xyz"]\n' +
        'SIGINT\n' +
        'EXIT [0,null]\n')
      t.end()
    })
  })

  t.test('SIGHUP', { skip: winNoSig }, function (t) {
    var child = cp.exec(process.execPath + ' ' + fixture + ' xyz', { shell: '/bin/bash' })

    var out = ''
    child.stdout.on('data', function (c) {
      out += c
      child.kill('SIGHUP')
    })
    child.on('close', function (code, signal) {
      t.equal(signal, 'SIGHUP')
      t.equal(out, 'WRAP ["{{FIXTURE}}","xyz"]\n' +
        '[]\n' +
        '["xyz"]\n' +
        'SIGHUP\n' +
        'EXIT [null,"SIGHUP"]\n')
      t.end()
    })
  })
})

t.test('exec shebang', { skip: winNoShebang }, function (t) {
  t.plan(3)

  t.test('basic', function (t) {
    var child = cp.exec(fixture + ' xyz', { shell: '/bin/bash' })

    var out = ''
    child.stdout.on('data', function (c) {
      out += c
    })
    child.on('close', function (code, signal) {
      t.equal(code, 0)
      t.equal(signal, null)
      t.equal(out, expect)
      t.end()
    })
  })

  t.test('SIGHUP', function (t) {
    var child = cp.exec(fixture + ' xyz', { shell: '/bin/bash' })

    var out = ''
    child.stdout.on('data', function (c) {
      out += c
      child.kill('SIGHUP')
    })
    child.on('close', function (code, signal) {
      t.equal(signal, 'SIGHUP')
      t.equal(out, 'WRAP ["{{FIXTURE}}","xyz"]\n' +
        '[]\n' +
        '["xyz"]\n' +
        'SIGHUP\n' +
        'EXIT [null,"SIGHUP"]\n')
      t.end()
    })
  })

  t.test('SIGINT', function (t) {
    var child = cp.exec(fixture + ' xyz', { shell: '/bin/bash' })

    var out = ''
    child.stdout.on('data', function (c) {
      out += c
    })
    child.stdout.once('data', function () {
      child.kill('SIGINT')
    })
    child.stderr.on('data', function (t) {
      console.error(t)
    })
    child.on('close', function (code, signal) {
      t.equal(code, 0)
      t.equal(signal, null)
      t.equal(out, 'WRAP ["{{FIXTURE}}","xyz"]\n' +
        '[]\n' +
        '["xyz"]\n' +
        'SIGINT\n' +
        'EXIT [0,null]\n')
      t.end()
    })
  })
})

// see: https://github.com/bcoe/nyc/issues/190
t.test('Node 5.8.x + npm 3.7.x - spawn', { skip: winNoShebang }, function (t) {
  var npmdir = path.dirname(npmFixture)
  process.env.PATH = npmdir + ':' + (process.env.PATH || '')
  var child = cp.spawn('npm', ['xyz'])

  var out = ''
  child.stdout.on('data', function (c) {
    out += c
  })
  child.on('close', function (code, signal) {
    t.equal(code, 0)
    t.equal(signal, null)
    t.true(~out.indexOf('xyz'))
    t.end()
  })
})

t.test('Node 5.8.x + npm 3.7.x - shell', { skip: winNoShebang }, function (t) {
  var npmdir = path.dirname(npmFixture)
  process.env.PATH = npmdir + ':' + (process.env.PATH || '')
  var child = cp.exec('npm xyz')

  var out = ''
  child.stdout.on('data', function (c) {
    out += c
  })
  child.on('close', function (code, signal) {
    t.equal(code, 0)
    t.equal(signal, null)
    t.true(~out.indexOf('xyz'))
    t.end()
  })
})

t.test('--harmony', function (t) {
  var node = process.execPath
  var child = cp.spawn(node, ['--harmony', fixture, 'xyz'])
  var out = ''
  child.stdout.on('data', function (c) {
    out += c
  })
  child.on('close', function (code, signal) {
    t.equal(code, 0)
    t.equal(signal, null)
    t.equal(out, 'WRAP ["--harmony","{{FIXTURE}}","xyz"]\n' +
      '["--harmony"]\n' +
      '["xyz"]\n' +
      'EXIT [0,null]\n')
    t.end()
  })
})

t.test('node exe with different name', function(t) {
  var fp = path.join(__dirname, 'fixtures', 'exething.exe')
  var data = fs.readFileSync(process.execPath)
  fs.writeFileSync(fp, data)
  fs.chmodSync(fp, '0775')
  var child = cp.spawn(process.execPath, [fixture, 'xyz'])

  var out = ''
  child.stdout.on('data', function (c) {
    out += c
  })
  child.on('close', function (code, signal) {
    t.equal(code, 0)
    t.equal(signal, null)
    t.equal(out, expect)
    fs.unlinkSync(fp)
    t.end()
  })
})

t.test('unwrap', function (t) {
  unwrap()
  t.end()
})
