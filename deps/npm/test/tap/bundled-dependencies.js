'use strict'
var test = require('tap').test
var common = require('../common-tap.js')
var path = require('path')
var rimraf = require('rimraf')
var mkdirp = require('mkdirp')
var fs = require('graceful-fs')
var tar = require('tar')
var zlib = require('zlib')
var basepath = path.resolve(__dirname, path.basename(__filename, '.js'))
var fixturepath = path.resolve(basepath, 'npm-test-bundled-deps')
var targetpath = path.resolve(basepath, 'target')
var Tacks = require('tacks')
var File = Tacks.File
var Dir = Tacks.Dir

test('basic bundling', function (t) {
  var fixture = new Tacks(
    Dir({
      'package.json': File({
        name: 'npm-test-files',
        version: '1.2.5',
        bundledDependencies: [
          'addme'
        ]
      }),
      node_modules: Dir({
        addme: Dir({
          'index.js': File('')
        }),
        iggyme: Dir({
          'index.js': File('')
        })
      })
    })
  )
  withFixture(t, fixture, function (done) {
    t.ok(fileExists('node_modules/addme'), 'bundled dep included')
    t.notOk(fileExists('node_modules/iggyme'), 'unspecified dep not included')
    done()
  })
})

test('scoped dep bundling', function (t) {
  var fixture = new Tacks(
    Dir({
      'package.json': File({
        name: 'npm-test-files',
        version: '1.2.5',
        bundledDependencies: [
          '@foo/addme'
        ]
      }),
      node_modules: Dir({
        '@foo': Dir({
          addme: Dir({
            'index.js': File('')
          }),
          iggyme: Dir({
            'index.js': File('')
          })
        })
      })
    })
  )
  withFixture(t, fixture, function (done) {
    t.ok(fileExists('node_modules/@foo/addme'), 'bundled dep included')
    t.notOk(
      fileExists('node_modules/@foo/iggyme'),
      'unspecified dep not included')
    done()
  })
})

function fileExists (file) {
  try {
    return !!fs.statSync(path.resolve(targetpath, 'package', file))
  } catch (_) {
    return false
  }
}

function withFixture (t, fixture, tester) {
  fixture.create(fixturepath)
  mkdirp.sync(targetpath)
  common.npm(['pack', fixturepath], {cwd: basepath}, extractAndCheck)
  function extractAndCheck (err, code) {
    if (err) throw err
    t.is(code, 0, 'pack went ok')
    extractTarball(checkTests)
  }
  function checkTests (err) {
    if (err) throw err
    tester(removeAndDone)
  }
  function removeAndDone (err) {
    if (err) throw err
    fixture.remove(fixturepath)
    rimraf.sync(basepath)
    t.done()
  }
}

function extractTarball (cb) {
  // Unpack to disk so case-insensitive filesystems are consistent
  fs.createReadStream(path.join(basepath, 'npm-test-files-1.2.5.tgz'))
    .pipe(zlib.Unzip())
    .on('error', cb)
    .pipe(tar.Extract(targetpath))
    .on('error', cb)
    .on('end', function () { cb() })
}
