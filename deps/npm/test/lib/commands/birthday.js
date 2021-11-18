const t = require('tap')
const { real: mockNpm } = require('../../fixtures/mock-npm')

t.test('birthday', async t => {
  t.plan(2)
  const { Npm } = mockNpm(t, {
    libnpmexec: ({ args, yes }) => {
      t.ok(yes)
      t.match(args, ['@npmcli/npm-birthday'])
    },
  })
  const npm = new Npm()
  await npm.exec('birthday', [])
})
