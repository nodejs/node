'use strict'

const test = require('tap').test
const fs = require('fs')
const path = require('path')
const http = require('http')
const https = require('https')
const install = require('../lib/install')
const semver = require('semver')
const devDir = require('./common').devDir()
const rimraf = require('rimraf')
const gyp = require('../lib/node-gyp')
const log = require('npmlog')

log.level = 'warn'

test('download over http', function (t) {
  t.plan(2)

  var server = http.createServer(function (req, res) {
    t.strictEqual(req.headers['user-agent'],
      'node-gyp v42 (node ' + process.version + ')')
    res.end('ok')
    server.close()
  })

  var host = 'localhost'
  server.listen(0, host, function () {
    var port = this.address().port
    var gyp = {
      opts: {},
      version: '42'
    }
    var url = 'http://' + host + ':' + port
    var req = install.test.download(gyp, {}, url)
    req.on('response', function (res) {
      var body = ''
      res.setEncoding('utf8')
      res.on('data', function (data) {
        body += data
      })
      res.on('end', function () {
        t.strictEqual(body, 'ok')
      })
    })
  })
})

test('download over https with custom ca', function (t) {
  t.plan(3)

  var cert = fs.readFileSync(path.join(__dirname, 'fixtures/server.crt'), 'utf8')
  var key = fs.readFileSync(path.join(__dirname, 'fixtures/server.key'), 'utf8')

  var cafile = path.join(__dirname, '/fixtures/ca.crt')
  var ca = install.test.readCAFile(cafile)
  t.strictEqual(ca.length, 1)

  var options = { ca: ca, cert: cert, key: key }
  var server = https.createServer(options, function (req, res) {
    t.strictEqual(req.headers['user-agent'],
      'node-gyp v42 (node ' + process.version + ')')
    res.end('ok')
    server.close()
  })

  server.on('clientError', function (err) {
    throw err
  })

  var host = 'localhost'
  server.listen(8000, host, function () {
    var port = this.address().port
    var gyp = {
      opts: { cafile: cafile },
      version: '42'
    }
    var url = 'https://' + host + ':' + port
    var req = install.test.download(gyp, {}, url)
    req.on('response', function (res) {
      var body = ''
      res.setEncoding('utf8')
      res.on('data', function (data) {
        body += data
      })
      res.on('end', function () {
        t.strictEqual(body, 'ok')
      })
    })
  })
})

test('download over http with proxy', function (t) {
  t.plan(2)

  var server = http.createServer(function (req, res) {
    t.strictEqual(req.headers['user-agent'],
      'node-gyp v42 (node ' + process.version + ')')
    res.end('ok')
    pserver.close(function () {
      server.close()
    })
  })

  var pserver = http.createServer(function (req, res) {
    t.strictEqual(req.headers['user-agent'],
      'node-gyp v42 (node ' + process.version + ')')
    res.end('proxy ok')
    server.close(function () {
      pserver.close()
    })
  })

  var host = 'localhost'
  server.listen(0, host, function () {
    var port = this.address().port
    pserver.listen(port + 1, host, function () {
      var gyp = {
        opts: {
          proxy: 'http://' + host + ':' + (port + 1)
        },
        version: '42'
      }
      var url = 'http://' + host + ':' + port
      var req = install.test.download(gyp, {}, url)
      req.on('response', function (res) {
        var body = ''
        res.setEncoding('utf8')
        res.on('data', function (data) {
          body += data
        })
        res.on('end', function () {
          t.strictEqual(body, 'proxy ok')
        })
      })
    })
  })
})

test('download over http with noproxy', function (t) {
  t.plan(2)

  var server = http.createServer(function (req, res) {
    t.strictEqual(req.headers['user-agent'],
      'node-gyp v42 (node ' + process.version + ')')
    res.end('ok')
    pserver.close(function () {
      server.close()
    })
  })

  var pserver = http.createServer(function (req, res) {
    t.strictEqual(req.headers['user-agent'],
      'node-gyp v42 (node ' + process.version + ')')
    res.end('proxy ok')
    server.close(function () {
      pserver.close()
    })
  })

  var host = 'localhost'
  server.listen(0, host, function () {
    var port = this.address().port
    pserver.listen(port + 1, host, function () {
      var gyp = {
        opts: {
          proxy: 'http://' + host + ':' + (port + 1),
          noproxy: 'localhost'
        },
        version: '42'
      }
      var url = 'http://' + host + ':' + port
      var req = install.test.download(gyp, {}, url)
      req.on('response', function (res) {
        var body = ''
        res.setEncoding('utf8')
        res.on('data', function (data) {
          body += data
        })
        res.on('end', function () {
          t.strictEqual(body, 'ok')
        })
      })
    })
  })
})

test('download with missing cafile', function (t) {
  t.plan(1)
  var gyp = {
    opts: { cafile: 'no.such.file' }
  }
  try {
    install.test.download(gyp, {}, 'http://bad/')
  } catch (e) {
    t.ok(/no.such.file/.test(e.message))
  }
})

test('check certificate splitting', function (t) {
  var cas = install.test.readCAFile(path.join(__dirname, 'fixtures/ca-bundle.crt'))
  t.plan(2)
  t.strictEqual(cas.length, 2)
  t.notStrictEqual(cas[0], cas[1])
})

// only run this test if we are running a version of Node with predictable version path behavior

test('download headers (actual)', function (t) {
  if (process.env.FAST_TEST ||
      process.release.name !== 'node' ||
      semver.prerelease(process.version) !== null ||
      semver.satisfies(process.version, '<10')) {
    return t.skip('Skipping actual download of headers due to test environment configuration')
  }

  t.plan(17)

  const expectedDir = path.join(devDir, process.version.replace(/^v/, ''))
  rimraf(expectedDir, (err) => {
    t.ifError(err)

    const prog = gyp()
    prog.parseArgv([])
    prog.devDir = devDir
    log.level = 'warn'
    install(prog, [], (err) => {
      t.ifError(err)

      fs.readFile(path.join(expectedDir, 'installVersion'), 'utf8', (err, data) => {
        t.ifError(err)
        t.strictEqual(data, '9\n', 'correct installVersion')
      })

      fs.readdir(path.join(expectedDir, 'include/node'), (err, list) => {
        t.ifError(err)

        t.ok(list.includes('common.gypi'))
        t.ok(list.includes('config.gypi'))
        t.ok(list.includes('node.h'))
        t.ok(list.includes('node_version.h'))
        t.ok(list.includes('openssl'))
        t.ok(list.includes('uv'))
        t.ok(list.includes('uv.h'))
        t.ok(list.includes('v8-platform.h'))
        t.ok(list.includes('v8.h'))
        t.ok(list.includes('zlib.h'))
      })

      fs.readFile(path.join(expectedDir, 'include/node/node_version.h'), 'utf8', (err, contents) => {
        t.ifError(err)

        const lines = contents.split('\n')

        // extract the 3 version parts from the defines to build a valid version string and
        // and check them against our current env version
        const version = ['major', 'minor', 'patch'].reduce((version, type) => {
          const re = new RegExp(`^#define\\sNODE_${type.toUpperCase()}_VERSION`)
          const line = lines.find((l) => re.test(l))
          const i = line ? parseInt(line.replace(/^[^0-9]+([0-9]+).*$/, '$1'), 10) : 'ERROR'
          return `${version}${type !== 'major' ? '.' : 'v'}${i}`
        }, '')

        t.strictEqual(version, process.version)
      })
    })
  })
})
