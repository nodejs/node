const t = require('tap')
const { load: loadMockNpm } = require('../../fixtures/mock-npm')

t.test('should prune using Arborist', async (t) => {
  t.plan(4)
  const { npm } = await loadMockNpm(t, {
    mocks: {
      '@npmcli/arborist': function (args) {
        t.ok(args, 'gets options object')
        t.ok(args.path, 'gets path option')
        this.prune = () => {
          t.ok(true, 'prune is called')
        }
      },
      '{LIB}/utils/reify-finish.js': (arb) => {
        t.ok(arb, 'gets arborist tree')
      },
    },
  })
  await npm.exec('prune', [])
})

t.test('prune threads allowScripts policy through to arborist', async t => {
  let capturedOpts
  const FakeArborist = function (opts) {
    capturedOpts = opts
    this.options = opts
    this.actualTree = { inventory: new Map() }
  }
  FakeArborist.prototype.prune = async () => {}

  const { npm } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'host',
        version: '1.0.0',
        allowScripts: { canvas: true },
      }),
    },
    mocks: {
      '@npmcli/arborist': FakeArborist,
      '{LIB}/utils/reify-finish.js': async () => {},
    },
  })
  await npm.exec('prune', [])
  t.strictSame(capturedOpts.allowScripts, { canvas: true },
    'opts.allowScripts populated from package.json')
})
