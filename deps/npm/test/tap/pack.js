'use strict'

const BB = require('bluebird')

const test = require('tap').test
const common = require('../common-tap')
const fs = BB.promisifyAll(require('graceful-fs'))
const path = require('path')
const rimraf = BB.promisify(require('rimraf'))
const Tacks = require('tacks')

const Dir = Tacks.Dir
const File = Tacks.File

const testDir = common.pkg
const tmp = path.join(testDir, 'tmp')
const cache = common.cache

test('basic pack', (t) => {
  const fixture = new Tacks(new Dir({
    'package.json': new File({
      name: 'generic-package',
      version: '90000.100001.5'
    })
  }))
  return rimraf(testDir)
    .then(() => fixture.create(testDir))
    .then(() => common.npm([
      'pack',
      '--loglevel', 'notice',
      '--cache', cache,
      '--tmp', tmp,
      '--prefix', testDir,
      '--no-global'
    ], {
      cwd: testDir
    }))
    .spread((code, stdout, stderr) => {
      t.equal(code, 0, 'npm pack exited ok')
      t.match(stderr, /notice\s+\d+[a-z]+\s+package\.json/gi, 'mentions package.json')
      t.match(stdout, /generic-package-90000\.100001\.5\.tgz/ig, 'found pkg')
      return fs.statAsync(
        path.join(testDir, 'generic-package-90000.100001.5.tgz')
      )
    })
    .then((stat) => t.ok(stat, 'tarball written to cwd'))
    .then(() => rimraf(testDir))
})

test('pack with bundled', (t) => {
  const fixture = new Tacks(new Dir({
    'package.json': new File({
      name: 'generic-package',
      version: '90000.100001.5',
      dependencies: {
        '@bundle/dep': '^1.0.0',
        'regular-dep': '^1.0.0'
      },
      bundleDependencies: [
        '@bundle/dep',
        'regular-dep'
      ]
    }),
    'node_modules': new Dir({
      'regular-dep': new Dir({
        'package.json': new File({
          name: 'regular-dep',
          version: '1.0.0'
        })
      }),
      '@bundle': new Dir({
        'dep': new Dir({
          'package.json': new File({
            name: '@bundle/dep',
            version: '1.0.0'
          })
        })
      })
    })
  }))
  return rimraf(testDir)
    .then(() => fixture.create(testDir))
    .then(() => common.npm([
      'pack',
      '--loglevel', 'notice',
      '--cache', cache,
      '--tmp', tmp,
      '--prefix', testDir,
      '--no-global'
    ], {
      cwd: testDir
    }))
    .spread((code, stdout, stderr) => {
      t.equal(code, 0, 'npm pack exited ok')
      t.match(stderr, /notice\s+\d+[a-z]+\s+package\.json/gi, 'mentions package.json')
      t.match(stderr, /notice\s+regular-dep/, 'regular dep mentioned')
      t.match(stderr, /notice\s+@bundle\/dep/, 'bundled dep mentioned')
    })
    .then(() => rimraf(testDir))
})

test('pack --dry-run', (t) => {
  const fixture = new Tacks(new Dir({
    'package.json': new File({
      name: 'generic-package',
      version: '90000.100001.5'
    })
  }))
  return rimraf(testDir)
    .then(() => fixture.create(testDir))
    .then(() => common.npm([
      'pack',
      '--dry-run',
      '--loglevel', 'notice',
      '--cache', cache,
      '--tmp', tmp,
      '--prefix', testDir,
      '--no-global'
    ], {
      cwd: testDir
    }))
    .spread((code, stdout, stderr) => {
      t.equal(code, 0, 'npm pack exited ok')
      t.match(stdout, /generic-package-90000\.100001\.5\.tgz/ig, 'found pkg')
      return fs.statAsync(
        path.join(testDir, 'generic-package-90000.100001.5.tgz')
      )
        .then(
          () => { throw new Error('should have failed') },
          (err) => t.equal(err.code, 'ENOENT', 'no tarball written!')
        )
    })
    .then(() => rimraf(testDir))
})

test('pack --json', (t) => {
  const fixture = new Tacks(new Dir({
    'package.json': new File({
      name: 'generic-package',
      version: '90000.100001.5'
    })
  }))
  return rimraf(testDir)
    .then(() => fixture.create(testDir))
    .then(() => common.npm([
      'pack',
      '--dry-run',
      '--json',
      '--loglevel', 'notice',
      '--cache', cache,
      '--tmp', tmp,
      '--prefix', testDir,
      '--no-global'
    ], {
      cwd: testDir
    }))
    .spread((code, stdout, stderr) => {
      t.equal(code, 0, 'npm pack exited ok')
      t.equal(stderr.trim(), '', 'no notice output')
      t.similar(JSON.parse(stdout), [{
        filename: 'generic-package-90000.100001.5.tgz',
        files: [{path: 'package.json'}],
        entryCount: 1
      }], 'pack details output as valid json')
    })
    .then(() => rimraf(testDir))
})

test('postpack', (t) => {
  const fixture = new Tacks(new Dir({
    'package.json': new File({
      name: 'generic-package',
      version: '90000.100001.5',
      scripts: {
        postpack: 'node -e "var fs = require(\'fs\'); fs.openSync(\'postpack-step\', \'w+\'); if (!fs.existsSync(\'generic-package-90000.100001.5.tgz\')) { throw new Error(\'tar archive does not exist on postpack\') }"'
      }
    })
  }))

  return rimraf(testDir)
    .then(() => fixture.create(testDir))
    .then(() => common.npm([
      'pack',
      '--loglevel', 'notice',
      '--cache', cache,
      '--tmp', tmp,
      '--prefix', testDir,
      '--no-global'
    ], {
      cwd: testDir
    }))
    .spread((code, stdout, stderr) => {
      t.equal(code, 0, 'npm pack exited ok')
      return fs.statAsync(
        path.join(testDir, 'postpack-step')
      )
    })
    .then(() => rimraf(testDir))
})
