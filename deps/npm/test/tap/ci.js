'use strict'

const BB = require('bluebird')

const common = BB.promisifyAll(require('../common-tap.js'))
const fs = BB.promisifyAll(require('fs'))
const mr = BB.promisify(require('npm-registry-mock'))
const path = require('path')
const rimraf = BB.promisify(require('rimraf'))
const Tacks = require('tacks')
const test = require('tap').test

const Dir = Tacks.Dir
const File = Tacks.File
const testDir = path.join(__dirname, 'ci')

const EXEC_OPTS = { cwd: testDir }

const PKG = {
  name: 'top',
  version: '1.2.3',
  dependencies: {
    optimist: '0.6.0',
    clean: '2.1.6'
  }
}
let RAW_LOCKFILE
let SERVER
let TREE

function scrubFrom (tree) {
  // npm ci and npm i write different `from` fields for dependency deps. This
  // is fine any ok, but it messes with `t.deepEqual` comparisons.
  function _scrubFrom (deps) {
    Object.keys(deps).forEach((k) => {
      deps[k].from = ''
      if (deps[k].dependencies) { _scrubFrom(deps[k].dependencies) }
    })
  }
  tree.dependencies && _scrubFrom(tree.dependencies)
}

test('setup', () => {
  const fixture = new Tacks(Dir({
    'package.json': File(PKG)
  }))
  fixture.create(testDir)
  return mr({port: common.port})
    .then((server) => {
      SERVER = server
      return common.npm([
        'install',
        '--registry', common.registry
      ], EXEC_OPTS)
        .then(() => fs.readFileAsync(
          path.join(testDir, 'package-lock.json'),
          'utf8')
        )
        .then((lock) => {
          RAW_LOCKFILE = lock
        })
        .then(() => common.npm(['ls', '--json'], EXEC_OPTS))
        .then((ret) => {
          TREE = scrubFrom(JSON.parse(ret[1]))
        })
    })
})

test('basic installation', (t) => {
  const fixture = new Tacks(Dir({
    'package.json': File(PKG),
    'package-lock.json': File(RAW_LOCKFILE)
  }))
  return rimraf(testDir)
    .then(() => fixture.create(testDir))
    .then(() => common.npm([
      'ci',
      '--registry', common.registry,
      '--loglevel', 'warn'
    ], EXEC_OPTS))
    .then((ret) => {
      const code = ret[0]
      const stdout = ret[1]
      const stderr = ret[2]
      t.equal(code, 0, 'command completed without error')
      t.equal(stdout.trim(), '', 'no output on stdout')
      t.match(
        stderr.trim(),
        /^added 6 packages in \d+(?:\.\d+)?s$/,
        'no warnings on stderr, and final output has right number of packages'
      )
      return fs.readdirAsync(path.join(testDir, 'node_modules'))
    })
    .then((modules) => {
      t.deepEqual(modules.sort(), [
        'async', 'checker', 'clean', 'minimist', 'optimist', 'wordwrap'
      ], 'packages installed')
      return BB.all(modules.map((mod) => {
        return fs.readFileAsync(
          path.join(testDir, 'node_modules', mod, 'package.json')
        )
          .then((f) => JSON.parse(f))
          .then((pkgjson) => {
            t.equal(pkgjson.name, mod, `${mod} package name correct`)
            t.match(
              pkgjson._integrity,
              /sha\d+-[a-z0-9=+/]+$/i,
              `${mod} pkgjson has _integrity`
            )
            t.match(
              pkgjson._resolved,
              new RegExp(`http.*/-/${mod}-${pkgjson.version}.tgz`),
              `${mod} pkgjson has correct _resolved`
            )
            t.match(
              pkgjson._from,
              new RegExp(`${mod}@.*`),
              `${mod} pkgjson has _from field`
            )
          })
      }))
    })
    .then(() => fs.readFileAsync(
      path.join(testDir, 'package-lock.json'),
      'utf8')
    )
    .then((lock) => t.equal(lock, RAW_LOCKFILE, 'package-lock.json unchanged'))
    .then(() => common.npm(['ls', '--json'], EXEC_OPTS))
    .then((ret) => {
      const lsResult = JSON.parse(ret[1])
      t.equal(ret[0], 0, 'ls exited successfully')
      t.deepEqual(scrubFrom(lsResult), TREE, 'tree matches one from `install`')
    })
})

test('supports npm-shrinkwrap.json as well', (t) => {
  const fixture = new Tacks(Dir({
    'package.json': File(PKG),
    'npm-shrinkwrap.json': File(RAW_LOCKFILE)
  }))
  return rimraf(testDir)
    .then(() => fixture.create(testDir))
    .then(() => common.npm([
      'ci',
      '--registry', common.registry,
      '--loglevel', 'warn'
    ], EXEC_OPTS))
    .then((ret) => {
      const code = ret[0]
      const stdout = ret[1]
      const stderr = ret[2]
      t.equal(code, 0, 'command completed without error')
      t.equal(stdout.trim(), '', 'no output on stdout')
      t.match(
        stderr.trim(),
        /^added 6 packages in \d+(?:\.\d+)?s$/,
        'no warnings on stderr, and final output has right number of packages'
      )
    })
    .then(() => common.npm(['ls', '--json'], EXEC_OPTS))
    .then((ret) => {
      t.equal(ret[0], 0, 'ls exited successfully')
      t.deepEqual(
        scrubFrom(JSON.parse(ret[1])),
        TREE,
        'tree matches one from `install`'
      )
    })
    .then(() => fs.readFileAsync(
      path.join(testDir, 'npm-shrinkwrap.json'),
      'utf8')
    )
    .then((lock) => t.equal(lock, RAW_LOCKFILE, 'npm-shrinkwrap.json unchanged'))
    .then(() => fs.readdirAsync(path.join(testDir)))
    .then((files) => t.notOk(
      files.some((f) => f === 'package-lock.json'),
      'no package-lock.json created'
    ))
})

test('removes existing node_modules/ before installing', (t) => {
  const fixture = new Tacks(Dir({
    'package.json': File(PKG),
    'package-lock.json': File(RAW_LOCKFILE),
    'node_modules': Dir({
      foo: Dir({
        'index.js': File('"hello world"')
      })
    })
  }))
  return rimraf(testDir)
    .then(() => fixture.create(testDir))
    .then(() => common.npm([
      'ci',
      '--registry', common.registry,
      '--loglevel', 'warn'
    ], EXEC_OPTS))
    .then((ret) => {
      const code = ret[0]
      const stdout = ret[1]
      const stderr = ret[2]
      t.equal(code, 0, 'command completed without error')
      t.equal(stdout.trim(), '', 'no output on stdout')
      t.match(
        stderr.trim(),
        /^npm.*WARN.*removing existing node_modules/,
        'user warned that existing node_modules were removed'
      )
      return fs.readdirAsync(path.join(testDir, 'node_modules'))
    })
    .then((modules) => {
      t.deepEqual(modules.sort(), [
        'async', 'checker', 'clean', 'minimist', 'optimist', 'wordwrap'
      ], 'packages installed, with old node_modules dir gone')
    })
    .then(() => common.npm(['ls'], EXEC_OPTS))
    .then((ret) => t.equal(ret[0], 0, 'ls exited successfully'))
    .then(() => fs.readFileAsync(
      path.join(testDir, 'package-lock.json'),
      'utf8')
    )
    .then((lock) => t.equal(lock, RAW_LOCKFILE, 'package-lock.json unchanged'))
})

test('installs all package types correctly')

test('errors if package-lock.json missing', (t) => {
  const fixture = new Tacks(Dir({
    'package.json': File(PKG)
  }))
  return rimraf(testDir)
    .then(() => fixture.create(testDir))
    .then(() => common.npm([
      'ci',
      '--registry', common.registry,
      '--loglevel', 'warn'
    ], EXEC_OPTS))
    .then((ret) => {
      const code = ret[0]
      const stdout = ret[1]
      const stderr = ret[2]
      t.equal(code, 1, 'command errored')
      t.equal(stdout.trim(), '', 'no output on stdout')
      t.match(
        stderr.trim(),
        /can only install packages with an existing package-lock/i,
        'user informed about the issue'
      )
      return fs.readdirAsync(path.join(testDir))
    })
    .then((dir) => {
      t.notOk(dir.some((f) => f === 'node_modules'), 'no node_modules installed')
      t.notOk(
        dir.some((f) => f === 'package-lock.json'),
        'no package-lock.json created'
      )
    })
})

test('errors if package-lock.json invalid', (t) => {
  const badJson = JSON.parse(RAW_LOCKFILE)
  delete badJson.dependencies.optimist
  const fixture = new Tacks(Dir({
    'package.json': File(PKG),
    'package-lock.json': File(badJson)
  }))
  return rimraf(testDir)
    .then(() => fixture.create(testDir))
    .then(() => common.npm([
      'ci',
      '--registry', common.registry,
      '--loglevel', 'warn'
    ], EXEC_OPTS))
    .then((ret) => {
      const code = ret[0]
      const stdout = ret[1]
      const stderr = ret[2]
      t.equal(code, 1, 'command errored')
      t.equal(stdout.trim(), '', 'no output on stdout')
      t.match(
        stderr.trim(),
        /can only install packages when your package.json/i,
        'user informed about the issue'
      )
      return fs.readdirAsync(path.join(testDir))
    })
    .then((dir) => {
      t.notOk(dir.some((f) => f === 'node_modules'), 'no node_modules installed')
    })
    .then(() => fs.readFileAsync(
      path.join(testDir, 'package-lock.json'),
      'utf8')
    )
    .then((lock) => t.deepEqual(
      JSON.parse(lock),
      badJson,
      'bad package-lock.json left unchanged')
    )
})

test('cleanup', () => {
  SERVER.close()
  return rimraf(testDir)
})
