'use strict'
const path = require('path')
const test = require('tap').test
const Tacks = require('tacks')
const File = Tacks.File
const Dir = Tacks.Dir
const common = require('../common-tap.js')
const fs = require('fs')

const basedir = common.pkg
const testdir = path.join(basedir, 'testdir')
const cachedir = common.cache
const globaldir = path.join(basedir, 'global')
const tmpdir = path.join(basedir, 'tmp')
const optionaldir = path.join(testdir, 'optional')
const devdir = path.join(testdir, 'dev')

const env = common.newEnv().extend({
  npm_config_cache: cachedir,
  npm_config_tmp: tmpdir,
  npm_config_prefix: globaldir,
  npm_config_registry: common.registry,
  npm_config_loglevel: 'error'
})

/**
 * NOTE: Tarball Fixtures
 * They contain package.json files with dependencies like the following:
 * a-1.0.0.tgz: package/package.json
 * {
 *   "name":"a",
 *   "version":"1.0.0",
 *   "dependencies":{
 *     "b":"./b-1.0.0.tgz"
 *   }
 * }
 * example-1.0.0.tgz: package/package.json
 * {
 *   "name":"example",
 *   "version":"1.0.0",
 *   "dependencies":{
 *     "a":"./a-1.0.0.tgz",
 *     "b":"./b-1.0.0.tgz"
 *   }
 * }
 */
const fixture = new Tacks(Dir({
  cache: Dir(),
  global: Dir(),
  tmp: Dir(),
  testdir: Dir({
    'a-1.0.0.tgz': File(Buffer.from(
      '1f8b0800000000000003edcfc10a83300c06e09df71492f35653567bf06d' +
      'a2067163b558b7c3c4775f54f0e4654c18837e973f4da0249eca1bd59cfa' +
      '25d535b4eeb03344b4c6245bfd8946995d328b5a5b3bd55264464beebdc8' +
      '9647e8a99355befd67b92559f34f0ce0e8ce9003c1099edc85a675f2d20a' +
      '154aa762cfae6257361c201fa090994a8bf33c577dfd82713cfefa86288a' +
      'a2e8736f68a0ff4400080000',
      'hex'
    )),
    'b-1.0.0.tgz': File(Buffer.from(
      '1f8b08000000000000032b484cce4e4c4fd52f80d07a59c5f9790c540606' +
      '06066626260ad8c4c1c0d85c81c1d8d4ccc0d0d0cccc00a80ec830353103' +
      'd2d4760836505a5c925804740aa5e640bca200a78708a856ca4bcc4d55b2' +
      '524a52d2512a4b2d2acecccf03f20cf50cf40c946ab906da79a360148c82' +
      '51300a680400106986b400080000',
      'hex'
    )),
    dev: Dir({
      'package.json': File({
        name: 'dev',
        version: '1.0.0',
        devDependencies: {
          example: '../example-1.0.0.tgz'
        }
      })
    }),
    'example-1.0.0.tgz': File(Buffer.from(
      '1f8b0800000000000003ed8fc10a83300c863def2924e7ada6587bf06daa' +
      '06719bb55837c6c4775fa6307670a70963d0ef92f02584fcce94275353e2' +
      '962a8ebeb3d1c620a2562a5ef34f64aae328cd344aa935f21e379962875b' +
      '3fb2c6c50fa6e757bebdb364895ff54f18c19a962007ba99d69d09f670a5' +
      'de379d6527050a645391235b912d1bf2908f607826127398e762a8efbc53' +
      'ccae7873d3b4fb75ba402010087ce2014747c9d500080000',
      'hex'
    )),
    optional: Dir({
      'package.json': File({
        name: 'optional',
        version: '1.0.0',
        optionalDependencies: {
          example: '../example-1.0.0.tgz'
        }
      })
    })
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
  return common.fakeRegistry.listen()
})

test('optional dependency identification', function (t) {
  return common.npm(['install', '--no-optional'], {cwd: optionaldir, env}).then(([code, stdout, stderr]) => {
    t.is(code, 0, 'no error code')
    t.is(stderr, '', 'no error output')
    t.notOk(fs.existsSync(path.join(optionaldir, 'node_modules')), 'did not install anything')
    t.similar(JSON.parse(fs.readFileSync(path.join(optionaldir, 'package-lock.json'), 'utf8')), {
      dependencies: {
        a: {
          version: 'file:../a-1.0.0.tgz',
          optional: true
        },
        b: {
          version: 'file:../b-1.0.0.tgz',
          optional: true
        },
        example: {
          version: '1.0.0',
          optional: true
        }
      }
    }, 'locks dependencies as optional')
  })
})

test('development dependency identification', function (t) {
  return common.npm(['install', '--only=prod'], {cwd: devdir, env}).then(([code, stdout, stderr]) => {
    t.is(code, 0, 'no error code')
    t.is(stderr, '', 'no error output')
    t.notOk(fs.existsSync(path.join(devdir, 'node_modules')), 'did not install anything')
    t.similar(JSON.parse(fs.readFileSync(path.join(devdir, 'package-lock.json'), 'utf8')), {
      dependencies: {
        a: {
          version: 'file:../a-1.0.0.tgz',
          dev: true
        },
        b: {
          version: 'file:../b-1.0.0.tgz',
          dev: true
        },
        example: {
          version: '1.0.0',
          dev: true
        }
      }
    }, 'locks dependencies as dev')
  })
})

test('default dependency identification', function (t) {
  return common.npm(['install'], {cwd: optionaldir, env}).then(([code, stdout, stderr]) => {
    t.is(code, 0, 'no error code')
    t.is(stderr, '', 'no error output')
    t.similar(JSON.parse(fs.readFileSync(path.join(optionaldir, 'package-lock.json'), 'utf8')), {
      dependencies: {
        a: {
          version: 'file:../a-1.0.0.tgz',
          optional: true
        },
        b: {
          version: 'file:../b-1.0.0.tgz',
          optional: true
        },
        example: {
          version: '1.0.0',
          optional: true
        }
      }
    }, 'locks dependencies as optional')
  })
})

test('cleanup', function (t) {
  common.fakeRegistry.close()
  cleanup()
  t.done()
})
