const common = require('../common-tap.js')
const Tacks = require('tacks')
const {File, Dir} = Tacks
const pkg = common.pkg
const t = require('tap')

// via https://github.com/chhetrisushil/npm-update-test
const goodPackage2Entry = {
  version: 'file:../package2',
  dev: true
}

const badPackage2Entry = {
  version: 'file:../package2',
  dev: true,
  dependencies: {
    '@test/package3': {
      version: '1.0.0'
    }
  }
}

const goodPackage1Deps = {
  '@test/package2': goodPackage2Entry,
  '@test/package3': {
    version: 'file:../package3',
    dev: true
  }
}

const badPackage1Deps = {
  '@test/package2': badPackage2Entry,
  '@test/package3': {
    version: 'file:../package3',
    dev: true
  }
}

const badPackage1Lock = {
  name: 'package1',
  version: '1.0.0',
  lockfileVersion: 1,
  requires: true,
  dependencies: badPackage1Deps
}

const goodPackage1Lock = {
  name: 'package1',
  version: '1.0.0',
  lockfileVersion: 1,
  requires: true,
  dependencies: goodPackage1Deps
}

const package2Lock = {
  name: 'package2',
  version: '1.0.0',
  lockfileVersion: 1,
  requires: true,
  dependencies: {
    '@test/package3': {
      version: 'file:../package3',
      dev: true
    }
  }
}

const package3Lock = {
  name: 'package3',
  version: '1.0.0',
  lockfileVersion: 1
}

t.test('setup fixture', t => {
  const fixture = new Tacks(new Dir({
    package1: new Dir({
      'package-lock.json': new File(badPackage1Lock),
      'package.json': new File({
        name: 'package1',
        version: '1.0.0',
        devDependencies: {
          '@test/package2': 'file:../package2',
          '@test/package3': 'file:../package3'
        }
      })
    }),
    package2: new Dir({
      'package-lock.json': new File(package2Lock),
      'package.json': new File({
        name: 'package2',
        version: '1.0.0',
        devDependencies: {
          '@test/package3': 'file:../package3'
        }
      })
    }),
    package3: new Dir({
      'package-lock.json': new File(package3Lock),
      'package.json': new File({
        name: 'package3',
        version: '1.0.0'
      })
    })
  }))
  fixture.create(pkg)
  t.end()
})

t.test('install does not error', t =>
  common.npm(['install', '--no-registry'], { cwd: pkg + '/package1' })
    .then(([code, out, err]) => {
      t.equal(code, 0)
      console.error({out, err})
    }))

t.test('updated package-lock.json to good state, left others alone', t => {
  const fs = require('fs')
  const getlock = n => {
    const file = pkg + '/package' + n + '/package-lock.json'
    const lockdata = fs.readFileSync(file, 'utf8')
    return JSON.parse(lockdata)
  }
  t.strictSame(getlock(1), goodPackage1Lock)
  t.strictSame(getlock(2), package2Lock)
  t.strictSame(getlock(3), package3Lock)
  t.end()
})
