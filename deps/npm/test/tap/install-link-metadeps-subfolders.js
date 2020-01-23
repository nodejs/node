const t = require('tap')
const common = require('../common-tap.js')
const mkdirp = require('mkdirp')
const { writeFileSync, readFileSync } = require('fs')
const { resolve } = require('path')
const pkg = common.pkg
const app = resolve(pkg, 'app')
const lib = resolve(pkg, 'lib')
const moda = resolve(lib, 'module-a')
const modb = resolve(lib, 'module-b')

const rimraf = require('rimraf')

t.test('setup', t => {
  mkdirp.sync(app)
  mkdirp.sync(moda)
  mkdirp.sync(modb)

  writeFileSync(resolve(app, 'package.json'), JSON.stringify({
    name: 'app',
    version: '1.2.3',
    dependencies: {
      moda: 'file:../lib/module-a'
    }
  }))

  writeFileSync(resolve(moda, 'package.json'), JSON.stringify({
    name: 'moda',
    version: '1.2.3',
    dependencies: {
      modb: 'file:../module-b'
    }
  }))

  writeFileSync(resolve(modb, 'package.json'), JSON.stringify({
    name: 'modb',
    version: '1.2.3'
  }))

  t.end()
})

t.test('initial install to create package-lock',
  t => common.npm(['install'], { cwd: app })
    .then(([code]) => t.equal(code, 0, 'command worked')))

t.test('remove node_modules', t =>
  rimraf(resolve(pkg, 'node_modules'), t.end))

t.test('install again from package-lock', t =>
  common.npm(['install'], { cwd: app })
    .then(([code]) => {
      t.equal(code, 0, 'command worked')
      // verify that module-b is linked under module-a
      const depPkg = resolve(
        app,
        'node_modules',
        'moda',
        'node_modules',
        'modb',
        'package.json'
      )
      const data = JSON.parse(readFileSync(depPkg, 'utf8'))
      t.strictSame(data, {
        name: 'modb',
        version: '1.2.3'
      })
    }))
