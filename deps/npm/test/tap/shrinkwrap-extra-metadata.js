'use strict'

const common = require('../common-tap.js')
const fs = require('fs')
const mkdirp = require('mkdirp')
const mr = require('npm-registry-mock')
const npm = require('../../lib/npm.js')
const osenv = require('osenv')
const path = require('path')
const rimraf = require('rimraf')
const test = require('tap').test

const pkg = common.pkg

const json = {
  author: 'Rockbert',
  name: 'shrinkwrap-extra-metadata',
  version: '0.0.0'
}

test('setup', function (t) {
  cleanup()
  mkdirp.sync(pkg)
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(json, null, 2)
  )

  process.chdir(pkg)
  t.end()
})

test('adds additional metadata fields from the pkglock spec', function (t) {
  mr({ port: common.port }, function (er, s) {
    common.npm(
      [
        '--registry', common.registry,
        '--loglevel', 'silent',
        'shrinkwrap'
      ],
      { cwd: pkg, env: { NODE_PRESERVE_SYMLINKS: 'foo' } },
      function (err, code, stdout, stderr) {
        t.ifError(err, 'shrinkwrap ran without issue')
        t.notOk(code, 'shrinkwrap ran without raising error code')

        fs.readFile(path.resolve(pkg, 'npm-shrinkwrap.json'), function (err, desired) {
          t.ifError(err, 'read npm-shrinkwrap.json without issue')
          t.same(
            {
              'name': 'shrinkwrap-extra-metadata',
              'version': '0.0.0',
              'lockfileVersion': npm.lockfileVersion,
              'preserveSymlinks': 'foo'
            },
            JSON.parse(desired),
            'shrinkwrap wrote the expected metadata fields'
          )

          s.close()
          t.end()
        })
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
