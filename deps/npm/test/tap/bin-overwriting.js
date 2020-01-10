const t = require('tap')
const common = require('../common-tap.js')
const pkg = common.pkg

const { writeFileSync, readFileSync, readlink } = require('fs')
const readCmdShim = require('read-cmd-shim')
const path = require('path')
const readBinCb = process.platform === 'win32' ? readCmdShim : readlink
const readBin = bin => new Promise((resolve, reject) => {
  readBinCb(bin, (er, target) => {
    if (er) {
      reject(er)
    } else {
      resolve(path.resolve(path.dirname(bin), target))
    }
  })
})

// verify that we can't overwrite bins that we shouldn't be able to

const mkdirp = require('mkdirp').sync

process.env.npm_config_prefix = pkg + '/global'
process.env.npm_config_global = true

const globalBin = process.platform === 'win32'
  ? path.resolve(pkg, 'global')
  : path.resolve(pkg, 'global/bin')

const globalDir = process.platform === 'win32'
  ? path.resolve(pkg, 'global/node_modules')
  : path.resolve(pkg, 'global/lib/node_modules')

const beep = path.resolve(globalBin, 'beep')
const firstBin = path.resolve(globalDir, 'first/first.js')
const secondBin = path.resolve(globalDir, 'second/second.js')

t.test('setup', { bail: true }, t => {
  // set up non-link bin in place
  mkdirp(globalBin)
  writeFileSync(beep, 'beep boop')

  // create first package
  mkdirp(pkg + '/first')
  writeFileSync(pkg + '/first/package.json', JSON.stringify({
    name: 'first',
    version: '1.0.0',
    bin: { beep: 'first.js' }
  }))
  writeFileSync(pkg + '/first/first.js', `#!/usr/bin/env node
    console.log('first')`)

  // create second package
  mkdirp(pkg + '/second')
  writeFileSync(pkg + '/second/package.json', JSON.stringify({
    name: 'second',
    version: '1.0.0',
    bin: { beep: 'second.js' }
  }))
  writeFileSync(pkg + '/second/second.js', `#!/usr/bin/env node
    console.log('second')`)

  // pack both to install globally
  return common.npm(['pack'], { cwd: pkg + '/first' })
    .then(() => common.npm(['pack'], { cwd: pkg + '/second' }))
})

t.test('installing first fails, because pre-existing bin in place', t => {
  return common.npm([
    'install',
    pkg + '/first/first-1.0.0.tgz'
  ]).then(([code, stdout, stderr]) => {
    t.notEqual(code, 0)
    t.match(stderr, 'EEXIST')
    t.equal(readFileSync(beep, 'utf8'), 'beep boop')
  })
})

t.test('installing first with --force succeeds', t => {
  return common.npm([
    'install',
    pkg + '/first/first-1.0.0.tgz',
    '--force'
  ]).then(() => {
    return t.resolveMatch(readBin(beep), firstBin, 'bin written to first.js')
  })
})

t.test('installing second fails, because bin links to other package', t => {
  return common.npm([
    'install',
    pkg + '/second/second-1.0.0.tgz'
  ]).then(([code, stdout, stderr]) => {
    t.notEqual(code, 0)
    t.match(stderr, 'EEXIST')
    return t.resolveMatch(readBin(beep), firstBin, 'bin still linked to first')
  })
})

t.test('installing second with --force succeeds', t => {
  return common.npm([
    'install',
    pkg + '/second/second-1.0.0.tgz',
    '--force'
  ]).then(() => {
    return t.resolveMatch(readBin(beep), secondBin, 'bin written to second.js')
  })
})
