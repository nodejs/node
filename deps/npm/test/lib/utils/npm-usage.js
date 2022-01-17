const t = require('tap')
const { load: loadMockNpm } = require('../../fixtures/mock-npm.js')

t.test('usage', async t => {
  const { npm } = await loadMockNpm(t)
  t.afterEach(() => {
    npm.config.set('viewer', null)
    npm.config.set('long', false)
    npm.config.set('userconfig', '/some/config/file/.npmrc')
  })
  const { dirname } = require('path')
  const basedir = dirname(dirname(dirname(__dirname)))
  t.cleanSnapshot = str => str.split(basedir).join('{BASEDIR}')
    .split(require('../../../package.json').version).join('{VERSION}')

  npm.config.set('viewer', null)
  npm.config.set('long', false)
  npm.config.set('userconfig', '/some/config/file/.npmrc')

  t.test('basic usage', async t => {
    t.matchSnapshot(await npm.usage)
    t.end()
  })

  t.test('with browser', async t => {
    npm.config.set('viewer', 'browser')
    t.matchSnapshot(await npm.usage)
    t.end()
  })

  t.test('with long', async t => {
    npm.config.set('long', true)
    t.matchSnapshot(await npm.usage)
    t.end()
  })

  t.test('set process.stdout.columns', async t => {
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
      t.test(`columns=${cols}`, async t => {
        Object.defineProperty(process.stdout, 'columns', {
          value: cols,
          enumerable: true,
          configurable: true,
          writable: true,
        })
        t.matchSnapshot(await npm.usage)
      })
    }
  })
})
