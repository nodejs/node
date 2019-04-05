'use strict'

const BB = require('bluebird')

const common = require('../common-tap.js')
const mockTar = require('../util/mock-tarball.js')
const mr = common.fakeRegistry.compat
const path = require('path')
const rimraf = BB.promisify(require('rimraf'))
const Tacks = require('tacks')
const { test } = require('tap')

const { Dir, File } = Tacks

const testDir = path.join(__dirname, path.basename(__filename, '.js'))

let server
test('setup', t => {
  mr({}, (err, s) => {
    t.ifError(err, 'registry mocked successfully')
    server = s
    t.end()
  })
})

test('installs an npm package before a certain date', t => {
  const fixture = new Tacks(Dir({
    'package.json': File({})
  }))
  fixture.create(testDir)
  const packument = {
    name: 'foo',
    'dist-tags': { latest: '1.2.4' },
    versions: {
      '1.2.3': {
        name: 'foo',
        version: '1.2.3',
        dist: {
          tarball: `${server.registry}/foo/-/foo-1.2.3.tgz`
        }
      },
      '1.2.4': {
        name: 'foo',
        version: '1.2.4',
        dist: {
          tarball: `${server.registry}/foo/-/foo-1.2.4.tgz`
        }
      }
    },
    time: {
      created: '2017-01-01T00:00:01.000Z',
      modified: '2018-01-01T00:00:01.000Z',
      '1.2.3': '2017-01-01T00:00:01.000Z',
      '1.2.4': '2018-01-01T00:00:01.000Z'
    }
  }
  server.get('/foo').reply(200, packument)
  return mockTar({
    'package.json': JSON.stringify({
      name: 'foo',
      version: '1.2.3'
    })
  }).then(tarball => {
    server.get('/foo/-/foo-1.2.3.tgz').reply(200, tarball)
    server.get('/foo/-/foo-1.2.4.tgz').reply(500)
    return common.npm([
      'install', 'foo',
      '--before', '2018',
      '--json',
      '--cache', path.join(testDir, 'npmcache'),
      '--registry', server.registry
    ], { cwd: testDir })
  }).then(([code, stdout, stderr]) => {
    t.comment(stdout)
    t.comment(stderr)
    t.like(JSON.parse(stdout), {
      added: [{
        action: 'add',
        name: 'foo',
        version: '1.2.3'
      }]
    }, 'installed the 2017 version of the package')
  })
})

test('cleanup', t => {
  server.close()
  return rimraf(testDir)
})
