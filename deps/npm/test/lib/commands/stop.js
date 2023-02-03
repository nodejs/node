const t = require('tap')
const tspawk = require('../../fixtures/tspawk')
const { load: loadMockNpm } = require('../../fixtures/mock-npm')

const spawk = tspawk(t)

const isCmdRe = /(?:^|\\)cmd(?:\.exe)?$/i

t.test('should run stop script from package.json', async t => {
  const { npm } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'x',
        version: '1.2.3',
        scripts: {
          stop: 'node ./test-stop.js',
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
    ? ['/d', '/s', '/c', 'node ./test-stop.js foo']
    : ['-c', 'node ./test-stop.js foo']
  const script = spawk.spawn(scriptShell, scriptArgs)
  await npm.exec('stop', ['foo'])
  t.ok(script.called, 'script ran')
})
