const t = require('tap')
const tspawk = require('../../fixtures/tspawk')
const { load: loadMockNpm } = require('../../fixtures/mock-npm')

const spawk = tspawk(t)

const isCmdRe = /(?:^|\\)cmd(?:\.exe)?$/i

t.test('should run test script from package.json', async t => {
  const { npm } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'x',
        version: '1.2.3',
        scripts: {
          test: 'node ./test-test.js',
        },
      }),
    },
    config: {
      loglevel: 'silent',
      'script-shell': process.platform === 'win32' ? process.env.COMSPEC : 'sh',
    },
  })

  const scriptShell = npm.config.get('script-shell')
  const scriptArgs = isCmdRe.test(scriptShell)
    ? ['/d', '/s', '/c', 'node ./test-test.js foo']
    : ['-c', 'node ./test-test.js foo']
  const script = spawk.spawn(scriptShell, scriptArgs)
  await npm.exec('test', ['foo'])
  t.ok(script.called, 'script ran')
})
