'use strict'
const path = require('path')
const test = require('tap').test
const Tacks = require('tacks')
const File = Tacks.File
const Dir = Tacks.Dir
const common = require('../common-tap.js')

const basedir = path.join(__dirname, path.basename(__filename, '.js'))
const testdir = path.join(basedir, 'testdir')
const cachedir = path.join(basedir, 'cache')
const globaldir = path.join(basedir, 'global')
const tmpdir = path.join(basedir, 'tmp')

const conf = {
  cwd: testdir,
  env: common.newEnv().extend({
    npm_config_cache: cachedir,
    npm_config_tmp: tmpdir,
    npm_config_prefix: globaldir,
    npm_config_registry: common.registry,
    npm_config_loglevel: 'warn'
  })
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
        dep: 'file:dep-1.0.0.tgz',
        'peer-dep': 'file:peer-dep-2.0.0.tgz'
      }
    }),
    // Source dir
    // dep: Dir({
    //   'package.json': File({
    //     name: 'dep',
    //     version: '1.0.0',
    //     peerDependencies: {
    //       'peer-dep': 'file:peer-dep-1.0.0.tgz'
    //     }
    //   })
    // }),
    'dep-1.0.0.tgz': File(Buffer.from(
      '1f8b0800000000000003ed8f3d0ec2300c853be71451661a1cd166e8cc45' +
      'a2d654e1278d92c200eadd491a60ea462584946fb1f5fc9e655bd59e548f' +
      '5b9b2a3ffac1142b0300b2aae8921e1152d062574b10424a08bed0d4d10f' +
      '6b1fb2c4d58fca8553bedd937ea19ffa273c08a5cca80bb286b20e2ddb44' +
      'e186ceebc1444d70e090548be8f668d174685a8d3e8c63fc3529633a040e' +
      'fa8ccd5b28e7381ffb3b0bce894ce4d70f6732994c66e60975aec5690008' +
      '0000',
      'hex'
    )),
    // Source dir
    // 'peer-dep': Dir({
    //   'package.json': File({
    //     name: 'peer-dep',
    //     version: '2.0.0'
    //   })
    // }),
    'peer-dep-1.0.0.tgz': File(Buffer.from(
      '1f8b08000000000000032b484cce4e4c4fd52f80d07a59c5f9790c540606' +
      '06066626260ad8c4c1c0d45c81c1d8d4ccc0d0d0cccc00a80ec830353500' +
      'd2d4760836505a5c925804740aa5e640bca200a78708a8e6525050ca4bcc' +
      '4d55b252502a484d2dd24d492d50d2018996a5161567e6e781240cf50cf4' +
      '0c94b86ab906dab9a360148c8251300aa80400c1c67f6300080000',
      'hex'
    )),
    'peer-dep-2.0.0.tgz': File(Buffer.from(
      '1f8b08000000000000032b484cce4e4c4fd52f80d07a59c5f9790c540606' +
      '06066626260ad8c4c1c0d45c81c1d8d4ccc0d0d0cccc00a80ec830353500' +
      'd2d4760836505a5c925804740aa5e640bca200a78708a8e6525050ca4bcc' +
      '4d55b252502a484d2dd24d492d50d2018996a5161567e6e781248cf40cf4' +
      '0c94b86ab906dab9a360148c8251300aa80400cb30060800080000',
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

test('setup', t => {
  setup()
  return common.fakeRegistry.listen().then(() => common.npm(['install'], conf))
})

test('list warns about unmet peer dependency', t => {
  return common.npm(['ls'], conf).then(([code, stdout, stderr]) => {
    t.is(code, 1, 'command ran not ok')
    t.comment(stdout.trim())
    t.comment(stderr.trim())
    t.match(stdout, 'UNMET PEER DEPENDENCY peer-dep@2.0.0')
    t.match(stderr, 'npm ERR! peer dep missing: peer-dep@file:peer-dep-1.0.0.tgz, required by dep@1.0.0')
  })
})

test('list shows installed but unmet peer dependency', t => {
  return common.npm(['ls', 'peer-dep'], conf).then(([code, stdout, stderr]) => {
    t.is(code, 1, 'command ran not ok')
    t.comment(stdout.trim())
    t.comment(stderr.trim())
    t.match(stdout, 'UNMET PEER DEPENDENCY peer-dep@2.0.0')
    t.match(stderr, 'npm ERR! peer dep missing: peer-dep@file:peer-dep-1.0.0.tgz, required by dep@1.0.0')
  })
})

test('cleanup', t => {
  common.fakeRegistry.close()
  cleanup()
  t.done()
})
