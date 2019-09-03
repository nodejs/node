const t = require('tap')
const tar = require('tar')
const common = require('../common-tap.js')
const pkg = common.pkg
const rimraf = require('rimraf')
const { writeFileSync, statSync, chmodSync } = require('fs')
const { resolve } = require('path')
const mkdirp = require('mkdirp')

t.test('setup', t => {
  mkdirp.sync(resolve(pkg, 'package'))
  const pj = resolve(pkg, 'package', 'package.json')
  writeFileSync(pj, JSON.stringify({
    name: 'foo',
    version: '1.2.3'
  }))
  chmodSync(pj, 0o640)
  tar.c({
    sync: true,
    file: resolve(pkg, 'foo.tgz'),
    gzip: true,
    cwd: pkg
  }, ['package'])
  writeFileSync(resolve(pkg, 'package.json'), JSON.stringify({
    name: 'root',
    version: '1.2.3',
    dependencies: {
      foo: 'file:foo.tgz'
    }
  }))
  t.end()
})

t.test('run install to generate package-lock', t =>
  common.npm(['install'], { cwd: pkg }).then(([code]) => t.equal(code, 0)))

t.test('remove node_modules', t => rimraf(resolve(pkg, 'node_modules'), t.end))

t.test('run ci and check modes', t =>
  common.npm(['ci'], { cwd: pkg, stdio: 'inherit' }).then(([code]) => {
    t.equal(code, 0)
    const file = resolve(pkg, 'node_modules', 'foo', 'package.json')
    // bitwise AND against 0o705 so that we can detect whether
    // the file is world-readable.
    // Typical unix systems would leave the file 0o644
    // Travis-ci and some other Linux systems will be 0o664
    // Windows is 0o666
    // The regression this is detecting (ie, the default in the tarball)
    // leaves the file as 0o640.
    // Bitwise-AND 0o705 should always result in 0o604, and never 0o600
    const mode = statSync(file).mode & 0o705
    t.equal(mode, 0o604)
  }))
