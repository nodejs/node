const t = require('tap')
const glob = require('glob')
const { resolve } = require('path')
const { load: loadMockNpm } = require('../fixtures/mock-npm')

const full = process.env.npm_lifecycle_event === 'check-coverage'

if (!full) {
  t.pass('nothing to do here, not checking for full coverage')
} else {
  t.test('load all', async (t) => {
    const { npm } = await loadMockNpm(t, { })

    t.teardown(() => {
      const exitHandler = require('../../lib/utils/exit-handler.js')
      exitHandler.setNpm(npm)
      exitHandler()
    })

    t.test('load all the files', t => {
      // just load all the files so we measure coverage for the missing tests
      const dir = resolve(__dirname, '../../lib')
      for (const f of glob.sync(`${dir}/**/*.js`)) {
        require(f)
        t.pass('loaded ' + f)
      }
      t.pass('loaded all files')
      t.end()
    })
  })
}
