const t = require('tap')
const npm = require('../../../lib/npm.js')

t.test('usage', t => {
  t.afterEach(() => {
    npm.config.set('viewer', null)
    npm.config.set('long', false)
    npm.config.set('userconfig', '/some/config/file/.npmrc')
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
      t.matchSnapshot(npm.usage)
      t.end()
    })

    t.test('with browser', t => {
      npm.config.set('viewer', 'browser')
      t.matchSnapshot(npm.usage)
      t.end()
    })

    t.test('with long', t => {
      npm.config.set('long', true)
      t.matchSnapshot(npm.usage)
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
          t.matchSnapshot(npm.usage)
          t.end()
        })
      }
      t.end()
    })
    t.end()
  })
})
