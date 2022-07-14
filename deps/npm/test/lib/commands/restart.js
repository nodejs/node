const fs = require('fs')
const path = require('path')
const t = require('tap')
const tspawk = require('../../fixtures/tspawk')
const { load: loadMockNpm } = require('../../fixtures/mock-npm')

const spawk = tspawk(t)

// TODO this ... smells.  npm "script-shell" config mentions defaults but those
// are handled by run-script, not npm.  So for now we have to tie tests to some
// pretty specific internals of runScript
const makeSpawnArgs = require('@npmcli/run-script/lib/make-spawn-args.js')

t.test('should run restart script from package.json', async t => {
  const { npm } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'x',
        version: '1.2.3',
        scripts: {
          restart: 'node ./test-restart.js',
        },
      }),
    },
    config: {
      loglevel: 'silent',
    },
  })
  const [scriptShell] = makeSpawnArgs({ path: npm.prefix, cmd: 'node ./test-restart.js' })
  const script = spawk.spawn(scriptShell, (args) => {
    const lastArg = args[args.length - 1]
    const rightExtension = lastArg.endsWith('.cmd') || lastArg.endsWith('.sh')
    const rightFilename = path.basename(lastArg).startsWith('restart')
    const rightContents = fs.readFileSync(lastArg, { encoding: 'utf8' })
      .trim().endsWith('foo')
    return rightExtension && rightFilename && rightContents
  })
  await npm.exec('restart', ['foo'])
  t.ok(script.called, 'script ran')
})
