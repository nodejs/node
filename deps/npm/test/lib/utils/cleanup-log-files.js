const t = require('tap')

const glob = require('glob')
const rimraf = require('rimraf')
const mocks = { glob, rimraf }
const cleanup = t.mock('../../../lib/utils/cleanup-log-files.js', {
  glob: (...args) => mocks.glob(...args),
  rimraf: (...args) => mocks.rimraf(...args),
})
const { basename } = require('path')

const fs = require('fs')

t.test('clean up those files', t => {
  const cache = t.testdir({
    _logs: {
      '1-debug.log': 'hello',
      '2-debug.log': 'hello',
      '3-debug.log': 'hello',
      '4-debug.log': 'hello',
      '5-debug.log': 'hello',
    },
  })
  const warn = (...warning) => t.fail('failed cleanup', { warning })
  return cleanup(cache, 3, warn).then(() => {
    t.strictSame(fs.readdirSync(cache + '/_logs').sort(), [
      '3-debug.log',
      '4-debug.log',
      '5-debug.log',
    ])
  })
})

t.test('nothing to clean up', t => {
  const cache = t.testdir({
    _logs: {
      '4-debug.log': 'hello',
      '5-debug.log': 'hello',
    },
  })
  const warn = (...warning) => t.fail('failed cleanup', { warning })
  return cleanup(cache, 3, warn).then(() => {
    t.strictSame(fs.readdirSync(cache + '/_logs').sort(), [
      '4-debug.log',
      '5-debug.log',
    ])
  })
})

t.test('glob fail', t => {
  mocks.glob = (pattern, cb) => cb(new Error('no globbity'))
  t.teardown(() => mocks.glob = glob)
  const cache = t.testdir({})
  const warn = (...warning) => t.fail('failed cleanup', { warning })
  return cleanup(cache, 3, warn)
})

t.test('rimraf fail', t => {
  mocks.rimraf = (file, cb) => cb(new Error('youll never rimraf me!'))
  t.teardown(() => mocks.rimraf = rimraf)

  const cache = t.testdir({
    _logs: {
      '1-debug.log': 'hello',
      '2-debug.log': 'hello',
      '3-debug.log': 'hello',
      '4-debug.log': 'hello',
      '5-debug.log': 'hello',
    },
  })
  const warnings = []
  const warn = (...warning) => warnings.push(basename(warning[2]))
  return cleanup(cache, 3, warn).then(() => {
    t.strictSame(warnings.sort((a, b) => a.localeCompare(b, 'en')), [
      '1-debug.log',
      '2-debug.log',
    ])
  })
})
