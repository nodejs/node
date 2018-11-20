'use strict'

const BB = require('bluebird')

const common = require('../common-tap')
const fs = require('fs')
const mkdirp = require('mkdirp')
const mr = BB.promisify(require('npm-registry-mock'))
const path = require('path')
const rimraf = require('rimraf')
const test = require('tap').test

const testDir = path.join(__dirname, 'publish_test_package')

function setup () {
  cleanup()
  mkdirp.sync(path.join(testDir, 'cache'))

  fs.writeFileSync(
    path.join(testDir, 'package.json'),
    JSON.stringify({
      name: 'publish-organized',
      version: '1.2.5'
    }, null, 2),
    'utf8'
  )

  fs.writeFileSync(
    path.join(testDir, 'index.js'),
    'hello',
    'utf8'
  )
}

let port = common.port
function withServer (cb) {
  return mr({port: port++, throwOnUnmatched: true})
    .tap(cb)
    .then((server) => {
      server.done()
      return server.close()
    })
}

test('basic npm publish', (t) => {
  setup()
  return withServer((server) => {
    server.filteringRequestBody(verify)
      .put('/publish-organized', true)
      .reply(201, {ok: true})

    return common.npm([
      'publish',
      '--no-color',
      '--cache', path.join(testDir, 'cache'),
      '--registry=' + common.registry.replace(common.port, server.port),
      `--//localhost:${server.port}/:username=username`,
      `--//localhost:${server.port}/:_password=` + Buffer.from('password').toString('base64'),
      `--//localhost:${server.port}/:email=` + 'ogd@aoaioxxysz.net'
    ], {'cwd': testDir})
      .spread((code, stdout, stderr) => {
        t.comment(stdout)
        t.comment(stderr)
        t.is(code, 0, 'published without error')
      })

    function verify (body) {
      t.doesNotThrow(() => {
        const parsed = JSON.parse(body)
        const current = parsed.versions['1.2.5']
        t.equal(
          current._npmVersion,
          require(path.resolve(__dirname, '../../package.json')).version,
          'npm version is correct'
        )

        t.equal(
          current._nodeVersion,
          process.versions.node,
          'node version is correct'
        )
      }, 'converted body back into object')

      return true
    }
  })
})

test('npm publish --dry-run', (t) => {
  setup()
  return common.npm([
    'publish',
    '--dry-run',
    '--registry=https://example.registry/fake',
    '--cache', path.join(testDir, 'cache'),
    '--loglevel=notice',
    '--no-color'
  ], {'cwd': testDir})
    .spread((code, stdout, stderr) => {
      t.comment(stdout)
      t.comment(stderr)
      t.is(code, 0, 'published without error')
      t.match(stderr, /notice\s+\d+[a-z]+\s+package\.json/gi, 'mentions package.json')
      t.match(stderr, /notice\s+\d+[a-z]+\s+index\.js/gi, 'mentions index.js')
    })
})

test('npm publish --json', (t) => {
  setup()
  return withServer((server) => {
    server.filteringRequestBody(() => true)
      .put('/publish-organized', true)
      .reply(201, {ok: true})
    return common.npm([
      'publish',
      '--json',
      '--registry', common.registry.replace(common.port, server.port),
      '--cache', path.join(testDir, 'cache')
    ], {'cwd': testDir})
      .spread((code, stdout, stderr) => {
        t.comment(stdout)
        t.comment(stderr)
        t.is(code, 0, 'published without error')
        t.similar(JSON.parse(stdout), {
          name: 'publish-organized',
          version: '1.2.5',
          files: [
            {path: 'package.json'},
            {path: 'index.js'}
          ],
          entryCount: 2
        }, 'JSON output reflects package contents')
        t.equal(stderr.trim(), '', 'nothing on stderr')
      })
  })
})

test('npm publish --dry-run --json', (t) => {
  setup()
  return common.npm([
    'publish',
    '--dry-run',
    '--json',
    '--registry=https://example.registry/fake',
    '--cache', path.join(testDir, 'cache'),
    '--loglevel=notice',
    '--no-color'
  ], {'cwd': testDir})
    .spread((code, stdout, stderr) => {
      t.comment(stdout)
      t.comment(stderr)
      t.is(code, 0, 'published without error')
      t.similar(JSON.parse(stdout), {
        name: 'publish-organized',
        version: '1.2.5',
        files: [
          {path: 'package.json'},
          {path: 'index.js'}
        ],
        entryCount: 2
      }, 'JSON output reflects package contents')
      t.equal(stderr.trim(), '', 'nothing on stderr')
    })
})

test('cleanup', (t) => {
  cleanup()
  t.end()
})

function cleanup () {
  process.chdir(__dirname)
  rimraf.sync(testDir)
}
