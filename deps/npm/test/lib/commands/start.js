const t = require('tap')
const tspawk = require('../../fixtures/tspawk')
const { load: loadMockNpm } = require('../../fixtures/mock-npm')

const spawk = tspawk(t)

// TODO this ... smells.  npm "script-shell" config mentions defaults but those
// are handled by run-script, not npm.  So for now we have to tie tests to some
// pretty specific internals of runScript
const makeSpawnArgs = require('@npmcli/run-script/lib/make-spawn-args.js')

t.test('should run start script from package.json', async t => {
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
  const [scriptShell, scriptArgs] = makeSpawnArgs({ path: npm.prefix, cmd: 'node ./test-start.js' })
  // we're calling the script with 'foo' as an argument, so add that to the script
  let scriptContent = scriptArgs.pop()
  scriptContent += ' foo'
  scriptArgs.push(scriptContent)
  const script = spawk.spawn(scriptShell, scriptArgs)
  await npm.exec('start', ['foo'])
  t.ok(script.called, 'script ran')
})
