const t = require('tap')
const { pkg, npm } = require('../common-tap.js')
const { writeFileSync, statSync, readFileSync } = require('fs')
const mkdirp = require('mkdirp')
const proj = pkg + '/project'
const dep = pkg + '/dep'
mkdirp.sync(proj)
mkdirp.sync(dep)
writeFileSync(dep + '/package.json', JSON.stringify({
  name: 'dependency',
  version: '1.2.3'
}))
writeFileSync(proj + '/package.json', JSON.stringify({
  name: 'project',
  version: '4.2.0'
}))

const runTest = t => npm([
  'install',
  '../dep',
  '--no-registry'
], { cwd: proj }).then(([code, out, err]) => {
  t.equal(code, 0)
  t.match(out, /^\+ dependency@1\.2\.3\n.* 1 package in [0-9.]+m?s\n$/)
  t.equal(err, '')
  const data = readFileSync(proj + '/node_modules/dependency/package.json', 'utf8')
  t.same(JSON.parse(data), {
    name: 'dependency',
    version: '1.2.3'
  }, 'dep got installed')
  t.ok(statSync(proj + '/package-lock.json').isFile(), 'package-lock exists')
})

t.test('install without a registry, no package lock', t => runTest(t))
t.test('install without a registry, with package lock', t => runTest(t))
