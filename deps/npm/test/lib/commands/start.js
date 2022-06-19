const t = require('tap')
const tspawk = require('../../fixtures/tspawk')
const { load: loadMockNpm } = require('../../fixtures/mock-npm')

const spawk = tspawk(t)

// TODO this ... smells.  npm "script-shell" config mentions defaults but those
// are handled by run-script, not npm.  So for now we have to tie tests to some
// pretty specific internals of runScript
const makeSpawnArgs = require('@npmcli/run-script/lib/make-spawn-args.js')

t.test('should run start script from package.json', async t => {
  t.plan(2)
  const { npm } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'x',
        version: '1.2.3',
        scripts: {
          start: 'node ./test-start.js',
        },
      }),
    },
    config: {
      loglevel: 'silent',
    },
  })
  const [scriptShell] = makeSpawnArgs({ path: npm.prefix })
  const script = spawk.spawn(scriptShell, (args) => {
    t.ok(args.includes('node ./test-start.js "foo"'), 'ran start script with extra args')
    return true
  })
  await npm.exec('start', ['foo'])
  t.ok(script.called, 'script ran')
})
