const t = require('tap')

const OUTPUT = []
const output = (...msg) => OUTPUT.push(msg)
const requireInject = require('require-inject')
const usage = requireInject('../../../lib/utils/npm-usage.js', {
  '../../../lib/utils/output.js': output,
})
const npm = requireInject('../../../lib/npm.js')

t.test('usage', t => {
  t.afterEach((cb) => {
    npm.config.set('viewer', null)
    npm.config.set('long', false)
    npm.config.set('userconfig', '/some/config/file/.npmrc')
    cb()
  })
  const { dirname } = require('path')
  const basedir = dirname(dirname(dirname(__dirname)))
  t.cleanSnapshot = str => str.split(basedir).join('{BASEDIR}')
    .split(require('../../../package.json').version).join('{VERSION}')

  npm.load(err => {
    if (err)
      throw err

    npm.config.set('viewer', null)
    npm.config.set('long', false)
    npm.config.set('userconfig', '/some/config/file/.npmrc')

    t.test('basic usage', t => {
      usage(npm)
      t.equal(OUTPUT.length, 1)
      t.equal(OUTPUT[0].length, 1)
      t.matchSnapshot(OUTPUT[0][0])
      OUTPUT.length = 0
      t.end()
    })

    t.test('with browser', t => {
      npm.config.set('viewer', 'browser')
      usage(npm)
      t.equal(OUTPUT.length, 1)
      t.equal(OUTPUT[0].length, 1)
      t.matchSnapshot(OUTPUT[0][0])
      OUTPUT.length = 0
      npm.config.set('viewer', null)
      t.end()
    })

    t.test('with long', t => {
      npm.config.set('long', true)
      usage(npm)
      t.equal(OUTPUT.length, 1)
      t.equal(OUTPUT[0].length, 1)
      t.matchSnapshot(OUTPUT[0][0])
      OUTPUT.length = 0
      npm.config.set('long', false)
      t.end()
    })

    t.test('did you mean?', t => {
      npm.argv.push('unistnall')
      usage(npm)
      t.equal(OUTPUT.length, 2)
      t.equal(OUTPUT[0].length, 1)
      t.equal(OUTPUT[1].length, 1)
      t.matchSnapshot(OUTPUT[0][0])
      t.matchSnapshot(OUTPUT[1][0])
      OUTPUT.length = 0
      npm.argv.length = 0
      t.end()
    })

    t.test('did you mean?', t => {
      npm.argv.push('unistnall')
      const { exitCode } = process
      t.teardown(() => {
        if (t.passing())
          process.exitCode = exitCode
      })
      // make sure it fails when invalid
      usage(npm, false)
      t.equal(process.exitCode, 1)
      OUTPUT.length = 0
      npm.argv.length = 0
      t.end()
    })

    t.test('set process.stdout.columns', t => {
      const { columns } = process.stdout
      t.teardown(() => {
        Object.defineProperty(process.stdout, 'columns', {
          value: columns,
          enumerable: true,
          configurable: true,
          writable: true,
        })
      })
      const cases = [0, 90]
      for (const cols of cases) {
        t.test(`columns=${cols}`, t => {
          Object.defineProperty(process.stdout, 'columns', {
            value: cols,
            enumerable: true,
            configurable: true,
            writable: true,
          })
          usage(npm)
          t.equal(OUTPUT.length, 1)
          t.equal(OUTPUT[0].length, 1)
          t.matchSnapshot(OUTPUT[0][0])
          OUTPUT.length = 0
          t.end()
        })
      }
      t.end()
    })
    t.end()
  })
})
