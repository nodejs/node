var fs = require('fs')

if (process.argv[2] === 'write-builtin') {
  var pid = process.argv[3]
  fs.writeFileSync('npmrc', 'foo=bar\npid=' + pid + '\n')
  process.exit(0)
}

var common = require('../common-tap.js')
var path = require('path')
var rimraf = require('rimraf')
var mkdirp = require('mkdirp')
var folder = path.resolve(__dirname, 'builtin-config')
var test = require('tap').test
var npm = path.resolve(__dirname, '../..')
var spawn = require('child_process').spawn
var node = process.execPath

test('setup', function (t) {
  t.plan(1)
  rimraf.sync(folder)
  mkdirp.sync(folder + '/first')
  mkdirp.sync(folder + '/second')
  mkdirp.sync(folder + '/cache')
  mkdirp.sync(folder + '/tmp')

  t.pass('finished setup')
  t.end()
})

test('install npm into first folder', function (t) {
  t.plan(1)
  var args = ['install', npm, '-g',
              '--prefix=' + folder + '/first',
              '--ignore-scripts',
              '--cache=' + folder + '/cache',
              '--loglevel=silent',
              '--tmp=' + folder + '/tmp']
  common.npm(args, {stdio: 'inherit'}, function (er, code) {
    if (er) throw er
    t.equal(code, 0)
    t.end()
  })
})

test('write npmrc file', function (t) {
  t.plan(1)
  common.npm(['explore', 'npm', '-g',
              '--prefix=' + folder + '/first',
              '--cache=' + folder + '/cache',
              '--tmp=' + folder + '/tmp',
              '--',
              node, __filename, 'write-builtin', process.pid
             ],
             {'stdio': 'inherit'},
             function (er, code) {
               if (er) throw er
               t.equal(code, 0)
               t.end()
             })
})

test('use first npm to install second npm', function (t) {
  t.plan(3)
  // get the root location
  common.npm(
    [
      'root', '-g',
      '--prefix=' + folder + '/first',
      '--cache=' + folder + '/cache',
      '--tmp=' + folder + '/tmp'
    ],
    {},
    function (er, code, so) {
      if (er) throw er
      t.equal(code, 0)
      var root = so.trim()
      t.ok(fs.statSync(root).isDirectory())

      var bin = path.resolve(root, 'npm/bin/npm-cli.js')
      spawn(
        node,
        [
          bin,
          'install', npm,
          '-g',
          '--prefix=' + folder + '/second',
          '--cache=' + folder + '/cache',
          '--tmp=' + folder + '/tmp'
        ]
      )
      .on('error', function (er) { throw er })
      .on('close', function (code) {
        t.equal(code, 0, 'code is zero')
        t.end()
      })
    }
  )
})

test('verify that the builtin config matches', function (t) {
  t.plan(3)
  common.npm([ 'root', '-g',
               '--prefix=' + folder + '/first',
               '--cache=' + folder + '/cache',
               '--tmp=' + folder + '/tmp'
             ], {},
             function (er, code, so) {
               if (er) throw er
               t.equal(code, 0)
               var firstRoot = so.trim()
               common.npm([ 'root', '-g',
                            '--prefix=' + folder + '/second',
                            '--cache=' + folder + '/cache',
                            '--tmp=' + folder + '/tmp'
                          ], {},
                          function (er, code, so) {
                            if (er) throw er
                            t.equal(code, 0)
                            var secondRoot = so.trim()
                            var firstRc = path.resolve(firstRoot, 'npm', 'npmrc')
                            var secondRc = path.resolve(secondRoot, 'npm', 'npmrc')
                            var firstData = fs.readFileSync(firstRc, 'utf8')
                            var secondData = fs.readFileSync(secondRc, 'utf8')
                            t.equal(firstData, secondData)
                            t.end()
                          })
             })
})

test('clean', function (t) {
  rimraf.sync(folder)
  t.end()
})
