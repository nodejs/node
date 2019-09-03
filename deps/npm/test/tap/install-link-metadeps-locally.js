// XXX Remove in npm v7, when this is no longer how we do things
const t = require('tap')
const common = require('../common-tap.js')
const pkg = common.pkg
const mkdirp = require('mkdirp')
const { writeFileSync, statSync } = require('fs')
const { resolve } = require('path')
const mr = require('npm-registry-mock')
const rimraf = require('rimraf')

t.test('setup', t => {
  mkdirp.sync(resolve(pkg, 'node_modules'))
  mkdirp.sync(resolve(pkg, 'foo'))
  writeFileSync(resolve(pkg, 'foo', 'package.json'), JSON.stringify({
    name: 'foo',
    version: '1.2.3',
    dependencies: {
      underscore: '*'
    }
  }))

  writeFileSync(resolve(pkg, 'package.json'), JSON.stringify({
    name: 'root',
    version: '1.2.3',
    dependencies: {
      foo: 'file:foo'
    }
  }))

  mr({ port: common.port }, (er, s) => {
    if (er) {
      throw er
    }
    t.parent.teardown(() => s.close())
    t.end()
  })
})

t.test('initial install to create package-lock',
  t => common.npm(['install', '--registry', common.registry], { cwd: pkg })
    .then(([code]) => t.equal(code, 0, 'command worked')))

t.test('remove node_modules', t =>
  rimraf(resolve(pkg, 'node_modules'), t.end))

t.test('install again from package-lock', t =>
  common.npm(['install', '--registry', common.registry], { cwd: pkg })
    .then(([code]) => {
      t.equal(code, 0, 'command worked')
      const underscore = resolve(pkg, 'node_modules', 'underscore')
      t.equal(statSync(underscore).isDirectory(), true, 'underscore installed')
    }))
