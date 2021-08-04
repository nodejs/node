const t = require('tap')
const fs = require('fs')
const parseJSON = require('json-parse-even-better-errors')
const { fake: mockNpm } = require('../fixtures/mock-npm')
const { resolve } = require('path')

const flatOptions = {}
const npm = mockNpm(flatOptions)

const ERROR_OUTPUT = []
const WARN_OUTPUT = []
const SetScript = t.mock('../../lib/set-script.js', {
  npmlog: {
    error: (...args) => {
      ERROR_OUTPUT.push(args)
    },
    warn: (...args) => {
      WARN_OUTPUT.push(args)
    },
  },
})
const setScript = new SetScript(npm)

t.test('completion', t => {
  t.test('already have a script name', async t => {
    npm.localPrefix = t.testdir({})
    const res = await setScript.completion({conf: {argv: {remain: ['npm', 'run', 'x']}}})
    t.equal(res, undefined)
    t.end()
  })

  t.test('no package.json', async t => {
    npm.localPrefix = t.testdir({})
    const res = await setScript.completion({conf: {argv: {remain: ['npm', 'run']}}})
    t.strictSame(res, [])
    t.end()
  })

  t.test('has package.json, no scripts', async t => {
    npm.localPrefix = t.testdir({
      'package.json': JSON.stringify({}),
    })
    const res = await setScript.completion({conf: {argv: {remain: ['npm', 'run']}}})
    t.strictSame(res, [])
    t.end()
  })

  t.test('has package.json, with scripts', async t => {
    npm.localPrefix = t.testdir({
      'package.json': JSON.stringify({
        scripts: { hello: 'echo hello', world: 'echo world' },
      }),
    })
    const res = await setScript.completion({conf: {argv: {remain: ['npm', 'run']}}})
    t.strictSame(res, ['hello', 'world'])
    t.end()
  })

  t.end()
})

t.test('fails on invalid arguments', (t) => {
  t.plan(3)
  setScript.exec(['arg1'], (fail) => t.match(fail, /Expected 2 arguments: got 1/))
  setScript.exec(['arg1', 'arg2', 'arg3'], (fail) => t.match(fail, /Expected 2 arguments: got 3/))
  setScript.exec(['arg1', 'arg2', 'arg3', 'arg4'], (fail) => t.match(fail, /Expected 2 arguments: got 4/))
})

t.test('fails if run in postinstall script', (t) => {
  const lifecycleEvent = process.env.npm_lifecycle_event
  t.teardown(() => {
    process.env.npm_lifecycle_event = lifecycleEvent
  })

  process.env.npm_lifecycle_event = 'postinstall'
  t.plan(1)
  setScript.exec(['arg1', 'arg2'], (fail) => t.equal(fail.toString(), 'Error: Scripts can’t set from the postinstall script'))
})

t.test('fails when package.json not found', (t) => {
  t.plan(1)
  setScript.exec(['arg1', 'arg2'], (fail) => t.match(fail, /package.json not found/))
})

t.test('fails on invalid JSON', (t) => {
  npm.localPrefix = t.testdir({
    'package.json': 'iamnotjson',
  })

  t.plan(1)
  setScript.exec(['arg1', 'arg2'], (fail) => t.match(fail, /Invalid package.json: JSONParseError/))
})

t.test('creates scripts object', (t) => {
  npm.localPrefix = t.testdir({
    'package.json': '{}',
  })

  t.plan(2)
  setScript.exec(['arg1', 'arg2'], (error) => {
    t.equal(error, undefined)
    const contents = fs.readFileSync(resolve(npm.localPrefix, 'package.json'))
    t.ok(parseJSON(contents), {scripts: {arg1: 'arg2'}})
  })
})

t.test('warns when overwriting', (t) => {
  WARN_OUTPUT.length = 0
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({
      scripts: {
        arg1: 'blah',
      },
    }),
  })

  t.plan(2)
  setScript.exec(['arg1', 'arg2'], (error) => {
    t.equal(error, undefined, 'no error')
    t.hasStrict(WARN_OUTPUT[0], ['set-script', 'Script "arg1" was overwritten'], 'warning was logged')
  })
})

t.test('workspaces', (t) => {
  ERROR_OUTPUT.length = 0
  WARN_OUTPUT.length = 0
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'workspaces-test',
      version: '1.0.0',
      workspaces: ['workspace-a', 'workspace-b', 'workspace-c'],
    }),
    'workspace-a': {
      'package.json': '{}',
    },
    'workspace-b': {
      'package.json': '"notajsonobject"',
    },
    'workspace-c': {
      'package.json': JSON.stringify({
        scripts: {
          arg1: 'test',
        },
      }, null, ' '.repeat(6)).replace(/\n/g, '\r\n'),
    },
  })

  setScript.execWorkspaces(['arg1', 'arg2'], [], (error) => {
    t.equal(error, undefined, 'did not callback with an error')
    t.equal(process.exitCode, 1, 'did set the exitCode to 1')
    // force the exitCode back to 0 to make tap happy
    process.exitCode = 0

    // workspace-a had the script added
    const contentsA = fs.readFileSync(resolve(npm.localPrefix, 'workspace-a', 'package.json'))
    const dataA = parseJSON(contentsA)
    t.hasStrict(dataA, { scripts: { arg1: 'arg2' } }, 'defined the script')

    // workspace-b logged an error
    t.strictSame(ERROR_OUTPUT, [
      ['set-script', `Can't update invalid package.json data`],
      ['  in workspace: workspace-b'],
      [`  at location: ${resolve(npm.localPrefix, 'workspace-b')}`],
    ], 'logged workspace-b error')

    // workspace-c overwrite a script and logged a warning
    const contentsC = fs.readFileSync(resolve(npm.localPrefix, 'workspace-c', 'package.json'))
    const dataC = parseJSON(contentsC)
    t.hasStrict(dataC, { scripts: { arg1: 'arg2' } }, 'defined the script')
    t.equal(dataC[Symbol.for('indent')], ' '.repeat(6), 'kept the correct indent')
    t.equal(dataC[Symbol.for('newline')], '\r\n', 'kept the correct newline')
    t.match(WARN_OUTPUT, [
      ['set-script', 'Script "arg1" was overwritten'],
      ['  in workspace: workspace-c'],
      [`  at location: ${resolve(npm.localPrefix, 'workspace-c')}`],
    ], 'logged workspace-c warning')
    t.end()
  })
})
