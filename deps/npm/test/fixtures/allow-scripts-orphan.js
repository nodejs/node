const { workspaceMock } = require('./mock-npm.js')

module.exports = (t, { workspace = true, allowScripts, requiredScripts = false } = {}) => {
  const fixture = workspaceMock(t, {
    workspaces: { app: { 'abbrev@1.0.0': {} } },
  })
  const pkg = JSON.parse(fixture['package.json'])
  const lock = JSON.parse(fixture['package-lock.json'])

  if (!workspace) {
    delete pkg.workspaces
    pkg.dependencies = { app: 'file:app' }
    lock.packages[''] = { ...pkg }
  }
  pkg.allowScripts = allowScripts

  // The missing intermediate directory leaves this orphan in the ideal-tree inventory.
  lock.packages['app/node_modules/abbrev/node_modules/orphan'] = {
    version: '1.0.0',
    resolved: 'https://registry.npmjs.org/orphan/-/orphan-1.0.0.tgz',
    hasInstallScript: true,
    extraneous: true,
  }
  if (requiredScripts) {
    lock.packages['node_modules/abbrev'].hasInstallScript = true
  }

  delete fixture.node_modules
  fixture['package.json'] = JSON.stringify(pkg)
  fixture['package-lock.json'] = JSON.stringify(lock)
  return fixture
}
