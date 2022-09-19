const t = require('tap')
const { load: loadMockNpm } = require('../../fixtures/mock-npm')

t.test('birthday', async t => {
  t.plan(2)
  const { npm } = await loadMockNpm(t, {
    mocks: {
      libnpmexec: ({ args, yes }) => {
        t.ok(yes)
        t.match(args, ['@npmcli/npm-birthday'])
      },
    },
  })
  await npm.exec('birthday', [])
})
