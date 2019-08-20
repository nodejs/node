'use strict'
const fs = require('fs')
const path = require('path')
const test = require('tap').test
const requireInject = require('require-inject')
const Tacks = require('tacks')
const File = Tacks.File
const Dir = Tacks.Dir

const manifests = {
  'git-wrap': {
    name: 'git-wrap',
    version: '1.0.0',
    dependencies: {
      git: 'git+https://example.com/git#semver:1.0'
    }
  },
  git: {
    name: 'git',
    version: '1.0.0'
  }
}

const npm = requireInject.installGlobally('../../lib/npm.js', {
  pacote: {
    manifest: function (spec) {
      const manifest = manifests[spec.name]
      manifest._requested = spec
      manifest._resolved = spec.saveSpec.replace(/^file:/, '').replace(/(:?#.*)$/, '#0000000000000000000000000000000000000000')
      manifest._from = spec.rawSpec
      return Promise.resolve(manifest)
    }
  },
  '../../lib/utils/output.js': function () {
    // do not output to stdout
  }
})

const common = require('../common-tap.js')
const basedir = common.pkg
const testdir = path.join(basedir, 'testdir')
const cachedir = common.cache
const tmpdir = path.join(basedir, 'tmp')

const cwd = process.cwd()

const conf = {
  cache: cachedir,
  tmp: tmpdir,
  loglevel: 'silent',
  'package-lock-only': true
}

const fixture = new Tacks(Dir({
  cache: Dir(),
  global: Dir(),
  tmp: Dir(),
  testdir: Dir({
    'package.json': File({
      name: 'fixture',
      version: '1.0.0',
      dependencies: {
        git: 'git+https://example.com/git#semver:1',
        'git-wrap': 'file:git-wrap-1.0.0.tgz'
      }
    }),
    // Tarball source:
    // 'git-wrap': Dir({
    //   'package.json': File({
    //     name: 'git-wrap',
    //     version: '1.0.0',
    //     dependencies: {
    //       git: 'git+https://example.com/git#semver:1.0'
    //     }
    //   })
    // }),
    'git-wrap-1.0.0.tgz': File(Buffer.from(
      '1f8b0800000000000003ed8fcd0ac23010843df729423caaf9c13642df26' +
      'b44bad9a3434f107a4efeec68aa7de2c8898ef32cb0c3bec3a5d1d7503dc' +
      '8dca0ebeb38b991142a83c27537e44ee30db164a48a994c01987a210a873' +
      '1f32c5d907dde3299ff68cbf90b7fe08f78c106ab5015a12dab46173edb5' +
      'a3ebe85ea0f76d676320996062746b70606bb0550b1ea3b84f9e9baf82d5' +
      '3e04e74bcee1a68d3b01ab3ac3d15f7a30d8586215c59d211bb26fff9e48' +
      '2412ffcc034458283d00080000',
      'hex'
    ))
  })
}))

function setup () {
  cleanup()
  fixture.create(basedir)
}

function cleanup () {
  fixture.remove(basedir)
}

test('setup', function (t) {
  setup()
  process.chdir(testdir)
  npm.load(conf, function (err) {
    if (err) throw err
    t.done()
  })
})

test('dedupe matching git semver ranges', function (t) {
  npm.commands.install(function (err) {
    if (err) throw err
    const packageLock = JSON.parse(fs.readFileSync('package-lock.json'))
    t.same(packageLock, {
      name: 'fixture',
      version: '1.0.0',
      lockfileVersion: 1,
      requires: true,
      dependencies: {
        git: {
          from: 'git+https://example.com/git#semver:1',
          version: 'git+https://example.com/git#0000000000000000000000000000000000000000'
        },
        'git-wrap': {
          version: 'file:git-wrap-1.0.0.tgz',
          requires: {
            git: 'git+https://example.com/git#semver:1'
          }
        }
      }
    })
    t.done()
  })
})

test('cleanup', function (t) {
  process.chdir(cwd)
  cleanup()
  t.done()
})
