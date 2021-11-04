const t = require('tap')
const spawk = require('spawk')
const { real: mockNpm } = require('../../fixtures/mock-npm')

spawk.preventUnmatched()
t.teardown(() => {
  spawk.unload()
})

// TODO this ... smells.  npm "script-shell" config mentions defaults but those
// are handled by run-script, not npm.  So for now we have to tie tests to some
// pretty specific internals of runScript
const makeSpawnArgs = require('@npmcli/run-script/lib/make-spawn-args.js')

t.test('should run stop script from package.json', async t => {
  const prefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'x',
      version: '1.2.3',
      scripts: {
        restart: 'node ./test-restart.js',
      },
    }),
  })
  const { Npm } = mockNpm(t)
  const npm = new Npm()
  await npm.load()
  npm.log.level = 'silent'
  npm.localPrefix = prefix
  const [scriptShell] = makeSpawnArgs({ path: prefix })
  const script = spawk.spawn(scriptShell, (args) => {
    t.ok(args.includes('node ./test-restart.js "foo"'), 'ran stop script with extra args')
    return true
  })
  await npm.exec('restart', ['foo'])
  t.ok(script.called, 'script ran')
})
