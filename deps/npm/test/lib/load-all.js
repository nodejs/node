const t = require('tap')
const glob = require('glob')
const { resolve } = require('path')

const full = process.env.npm_lifecycle_event === 'check-coverage'

if (!full)
  t.pass('nothing to do here, not checking for full coverage')
else {
  // some files do config.get() on load, so have to load npm first
  const npm = require('../../lib/npm.js')
  t.test('load npm first', t => npm.load(t.end))

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

  t.test('call the exit handler so we dont freak out', t => {
    const exitHandler = require('../../lib/utils/exit-handler.js')
    exitHandler.setNpm(npm)
    exitHandler()
    t.end()
  })
}
