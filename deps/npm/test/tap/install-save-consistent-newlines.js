'use strict'

const fs = require('graceful-fs')
const path = require('path')

const mkdirp = require('mkdirp')
const mr = require('npm-registry-mock')
const rimraf = require('rimraf')
const test = require('tap').test

const common = require('../common-tap.js')

const pkg = common.pkg

const EXEC_OPTS = { cwd: pkg }

const json = {
  name: 'install-save-consistent-newlines',
  version: '0.0.1',
  description: 'fixture'
}

test('mock registry', function (t) {
  mr({ port: common.port }, function (er, s) {
    t.parent.teardown(() => s.close())
    t.end()
  })
})

const runTest = (t, opts) => {
  t.test('setup', setup(opts.ending))
  t.test('check', check(opts))
  t.end()
}

const setup = lineEnding => t => {
  rimraf(pkg, er => {
    if (er) {
      throw er
    }
    mkdirp.sync(path.resolve(pkg, 'node_modules'))

    var jsonStr = JSON.stringify(json, null, 2)

    if (lineEnding === '\r\n') {
      jsonStr = jsonStr.replace(/\n/g, '\r\n')
    }

    fs.writeFileSync(
      path.join(pkg, 'package.json'),
      jsonStr
    )

    t.end()
  })
}

const check = opts => t => common.npm(
  [
    '--loglevel', 'silent',
    '--registry', common.registry,
    '--save',
    'install', 'underscore@1.3.1'
  ],
  EXEC_OPTS
).then(([code, err, out]) => {
  t.notOk(code, 'npm install exited without raising an error code')

  const pkgPath = path.resolve(pkg, 'package.json')
  const pkgStr = fs.readFileSync(pkgPath, 'utf8')

  t.match(pkgStr, opts.match)
  t.notMatch(pkgStr, opts.notMatch)

  const pkgLockPath = path.resolve(pkg, 'package-lock.json')
  const pkgLockStr = fs.readFileSync(pkgLockPath, 'utf8')

  t.match(pkgLockStr, opts.match)
  t.notMatch(pkgLockStr, opts.notMatch)

  t.end()
})

test('keep LF line endings', t => {
  runTest(t, {
    ending: '\n',
    match: '\n',
    notMatch: '\r'
  })
})

test('keep CRLF line endings', t => {
  runTest(t, {
    ending: '\r\n',
    match: '\r\n',
    notMatch: /[^\r]\n/
  })
})
