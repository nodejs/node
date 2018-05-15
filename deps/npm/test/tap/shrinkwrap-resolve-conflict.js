'use strict'

const BB = require('bluebird')

const common = require('../common-tap.js')
const fs = BB.promisifyAll(require('fs'))
const path = require('path')
const rimraf = BB.promisify(require('rimraf'))
const test = require('tap').test
const Tacks = require('tacks')

const File = Tacks.File
const Dir = Tacks.Dir

const testDir = path.resolve(__dirname, path.basename(__filename, '.js'))
const modAdir = path.resolve(testDir, 'modA')
const modBdir = path.resolve(testDir, 'modB')
const modCdir = path.resolve(testDir, 'modC')

test('conflicts in shrinkwrap are auto-resolved on install', (t) => {
  const fixture = new Tacks(Dir({
    'package.json': File({
      name: 'foo',
      dependencies: {
        modA: 'file://' + modAdir,
        modB: 'file://' + modBdir
      },
      devDependencies: {
        modC: 'file://' + modCdir
      }
    }),
    'npm-shrinkwrap.json': File(
`
{
  "name": "foo",
  "requires": true,
  "lockfileVersion": 1,
  "dependencies": {
<<<<<<< HEAD
    "modA": {
      "version": "file:modA"
||||||| merged common ancestors
    "modB": {
      "version": "file:modB"
=======
    "modC": {
      "version": "file:modC",
      "dev": true
>>>>>>> branch
    }
  }
}
`),
    'modA': Dir({
      'package.json': File({
        name: 'modA',
        version: '1.0.0'
      })
    }),
    'modB': Dir({
      'package.json': File({
        name: 'modB',
        version: '1.0.0'
      })
    }),
    'modC': Dir({
      'package.json': File({
        name: 'modC',
        version: '1.0.0'
      })
    })
  }))
  fixture.create(testDir)
  function readJson (file) {
    return fs.readFileAsync(path.join(testDir, file)).then(JSON.parse)
  }
  return BB.fromNode((cb) => {
    common.npm([
      'install',
      '--loglevel', 'warn'
    ], {cwd: testDir}, (err, code, out, stderr) => {
      t.comment(stderr)
      t.match(stderr, /warn.*conflict/gi, 'warns about a conflict')
      cb(err || (code && new Error('non-zero exit code')) || null, out)
    })
  })
  .then(() => BB.join(
    readJson('npm-shrinkwrap.json'),
    readJson('node_modules/modA/package.json'),
    readJson('node_modules/modB/package.json'),
    readJson('node_modules/modC/package.json'),
    (lockfile, A, B, C) => {
      t.deepEqual(lockfile, {
        name: 'foo',
        requires: true,
        lockfileVersion: 1,
        dependencies: {
          modA: {
            version: 'file:modA'
          },
          modB: {
            version: 'file:modB'
          },
          modC: {
            version: 'file:modC',
            dev: true
          }
        }
      }, 'resolved lockfile matches expectations')
      t.equal(A.name, 'modA', 'installed modA')
      t.equal(B.name, 'modB', 'installed modB')
      t.equal(C.name, 'modC', 'installed modC')
    }
  ))
})

test('cleanup', () => rimraf(testDir))
