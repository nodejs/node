'use strict'
const path = require('path')
const test = require('tap').test
const Tacks = require('tacks')
const Dir = Tacks.Dir
const File = Tacks.File
const common = require('../common-tap.js')

const base = common.pkg
const singlePackage = path.join(base, 'single-funding-package')
const multiplePackages = path.join(base, 'top-level-funding')

function getFixturePackage ({ name, version, dependencies, funding }) {
  return Dir({
    'package.json': File({
      name,
      version: version || '1.0.0',
      funding: funding || {
        type: 'individual',
        url: 'http://example.com/donate'
      },
      dependencies: dependencies || {}
    })
  })
}

const fixture = new Tacks(Dir({
  'package.json': File({}),
  'single-funding-package': getFixturePackage({
    name: 'single-funding-package'
  }),
  'top-level-funding': getFixturePackage({
    name: 'top-level-funding',
    dependencies: {
      'dep-foo': 'file:../dep-foo',
      'dep-bar': 'file:../dep-bar'
    }
  }),
  'dep-foo': getFixturePackage({
    name: 'dep-foo',
    funding: {
      type: 'corporate',
      url: 'https://corp.example.com/sponsor'
    },
    dependencies: {
      'sub-dep-bar': 'file:../sub-dep-bar'
    }
  }),
  'dep-bar': getFixturePackage({
    name: 'dep-bar',
    version: '2.1.0',
    dependencies: {
      'sub-dep-bar': 'file:../sub-dep-bar'
    }
  }),
  'sub-dep-bar': getFixturePackage({
    name: 'sub-dep-bar',
    funding: {
      type: 'foo',
      url: 'http://example.com/foo'
    }
  })
}))

test('mention npm fund upon installing single dependency', function (t) {
  setup(t)
  common.npm(['install', '--no-save', singlePackage], {cwd: base}, function (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'installed successfully')
    t.is(stderr, '', 'no warnings')
    t.includes(stdout, '1 package is looking for funding', 'should print amount of packages needing funding')
    t.includes(stdout, '  run `npm fund` for details', 'should print npm fund mention')
    t.end()
  })
})

test('mention npm fund upon installing multiple dependencies', function (t) {
  setup(t)
  common.npm(['install', '--no-save', multiplePackages], {cwd: base}, function (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'installed successfully')
    t.is(stderr, '', 'no warnings')
    t.includes(stdout, '4 packages are looking for funding', 'should print amount of packages needing funding')
    t.includes(stdout, '  run `npm fund` for details', 'should print npm fund mention')
    t.end()
  })
})

test('skips mention npm fund using --no-fund option', function (t) {
  setup(t)
  common.npm(['install', '--no-save', '--no-fund', multiplePackages], {cwd: base}, function (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'installed successfully')
    t.is(stderr, '', 'no warnings')
    t.doesNotHave(stdout, '4 packages are looking for funding', 'should print amount of packages needing funding')
    t.doesNotHave(stdout, '  run `npm fund` for details', 'should print npm fund mention')
    t.end()
  })
})

test('mention packages looking for funding using --json', function (t) {
  setup(t)
  common.npm(['install', '--no-save', '--json', multiplePackages], {cwd: base}, function (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'installed successfully')
    t.is(stderr, '', 'no warnings')
    const res = JSON.parse(stdout)
    t.match(res.funding, '4 packages are looking for funding', 'should print amount of packages needing funding')
    t.end()
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function setup (t) {
  fixture.create(base)
  t.teardown(() => {
    cleanup()
  })
}

function cleanup () {
  fixture.remove(base)
}
