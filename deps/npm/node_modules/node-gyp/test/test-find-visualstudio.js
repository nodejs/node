'use strict'

const { describe, it } = require('mocha')
const assert = require('assert')
const fs = require('fs')
const path = require('path')
const findVisualStudio = require('../lib/find-visualstudio')
const VisualStudioFinder = findVisualStudio.test.VisualStudioFinder

const semverV1 = { major: 1, minor: 0, patch: 0 }

delete process.env.VCINSTALLDIR

function poison (object, property) {
  function fail () {
    console.error(Error(`Property ${property} should not have been accessed.`))
    process.abort()
  }
  var descriptor = {
    configurable: false,
    enumerable: false,
    get: fail,
    set: fail
  }
  Object.defineProperty(object, property, descriptor)
}

function TestVisualStudioFinder () { VisualStudioFinder.apply(this, arguments) }
TestVisualStudioFinder.prototype = Object.create(VisualStudioFinder.prototype)
// Silence npmlog - remove for debugging
TestVisualStudioFinder.prototype.log = {
  silly: () => {},
  verbose: () => {},
  info: () => {},
  warn: () => {},
  error: () => {}
}

describe('find-visualstudio', function () {
  it('VS2013', function () {
    const finder = new TestVisualStudioFinder(semverV1, null, (err, info) => {
      assert.strictEqual(err, null)
      assert.deepStrictEqual(info, {
        msBuild: 'C:\\MSBuild12\\MSBuild.exe',
        path: 'C:\\VS2013',
        sdk: null,
        toolset: 'v120',
        version: '12.0',
        versionMajor: 12,
        versionMinor: 0,
        versionYear: 2013
      })
    })

    finder.findVisualStudio2017OrNewer = (cb) => {
      finder.parseData(new Error(), '', '', cb)
    }
    finder.regSearchKeys = (keys, value, addOpts, cb) => {
      for (var i = 0; i < keys.length; ++i) {
        const fullName = `${keys[i]}\\${value}`
        switch (fullName) {
          case 'HKLM\\Software\\Microsoft\\VisualStudio\\SxS\\VC7\\14.0':
          case 'HKLM\\Software\\Wow6432Node\\Microsoft\\VisualStudio\\SxS\\VC7\\14.0':
            continue
          case 'HKLM\\Software\\Microsoft\\VisualStudio\\SxS\\VC7\\12.0':
            assert.ok(true, `expected search for registry value ${fullName}`)
            return cb(null, 'C:\\VS2013\\VC\\')
          case 'HKLM\\Software\\Microsoft\\MSBuild\\ToolsVersions\\12.0\\MSBuildToolsPath':
            assert.ok(true, `expected search for registry value ${fullName}`)
            return cb(null, 'C:\\MSBuild12\\')
          default:
            assert.fail(`unexpected search for registry value ${fullName}`)
        }
      }
      return cb(new Error())
    }
    finder.findVisualStudio()
  })

  it('VS2013 should not be found on new node versions', function () {
    const finder = new TestVisualStudioFinder({
      major: 10,
      minor: 0,
      patch: 0
    }, null, (err, info) => {
      assert.ok(/find .* Visual Studio/i.test(err), 'expect error')
      assert.ok(!info, 'no data')
    })

    finder.findVisualStudio2017OrNewer = (cb) => {
      const file = path.join(__dirname, 'fixtures', 'VS_2017_Unusable.txt')
      const data = fs.readFileSync(file)
      finder.parseData(null, data, '', cb)
    }
    finder.regSearchKeys = (keys, value, addOpts, cb) => {
      for (var i = 0; i < keys.length; ++i) {
        const fullName = `${keys[i]}\\${value}`
        switch (fullName) {
          case 'HKLM\\Software\\Microsoft\\VisualStudio\\SxS\\VC7\\14.0':
          case 'HKLM\\Software\\Wow6432Node\\Microsoft\\VisualStudio\\SxS\\VC7\\14.0':
            continue
          default:
            assert.fail(`unexpected search for registry value ${fullName}`)
        }
      }
      return cb(new Error())
    }
    finder.findVisualStudio()
  })

  it('VS2015', function () {
    const finder = new TestVisualStudioFinder(semverV1, null, (err, info) => {
      assert.strictEqual(err, null)
      assert.deepStrictEqual(info, {
        msBuild: 'C:\\MSBuild14\\MSBuild.exe',
        path: 'C:\\VS2015',
        sdk: null,
        toolset: 'v140',
        version: '14.0',
        versionMajor: 14,
        versionMinor: 0,
        versionYear: 2015
      })
    })

    finder.findVisualStudio2017OrNewer = (cb) => {
      finder.parseData(new Error(), '', '', cb)
    }
    finder.regSearchKeys = (keys, value, addOpts, cb) => {
      for (var i = 0; i < keys.length; ++i) {
        const fullName = `${keys[i]}\\${value}`
        switch (fullName) {
          case 'HKLM\\Software\\Microsoft\\VisualStudio\\SxS\\VC7\\14.0':
            assert.ok(true, `expected search for registry value ${fullName}`)
            return cb(null, 'C:\\VS2015\\VC\\')
          case 'HKLM\\Software\\Microsoft\\MSBuild\\ToolsVersions\\14.0\\MSBuildToolsPath':
            assert.ok(true, `expected search for registry value ${fullName}`)
            return cb(null, 'C:\\MSBuild14\\')
          default:
            assert.fail(`unexpected search for registry value ${fullName}`)
        }
      }
      return cb(new Error())
    }
    finder.findVisualStudio()
  })

  it('error from PowerShell', function () {
    const finder = new TestVisualStudioFinder(semverV1, null, null)

    finder.parseData(new Error(), '', '', (info) => {
      assert.ok(/use PowerShell/i.test(finder.errorLog[0]), 'expect error')
      assert.ok(!info, 'no data')
    })
  })

  it('empty output from PowerShell', function () {
    const finder = new TestVisualStudioFinder(semverV1, null, null)

    finder.parseData(null, '', '', (info) => {
      assert.ok(/use PowerShell/i.test(finder.errorLog[0]), 'expect error')
      assert.ok(!info, 'no data')
    })
  })

  it('output from PowerShell not JSON', function () {
    const finder = new TestVisualStudioFinder(semverV1, null, null)

    finder.parseData(null, 'AAAABBBB', '', (info) => {
      assert.ok(/use PowerShell/i.test(finder.errorLog[0]), 'expect error')
      assert.ok(!info, 'no data')
    })
  })

  it('wrong JSON from PowerShell', function () {
    const finder = new TestVisualStudioFinder(semverV1, null, null)

    finder.parseData(null, '{}', '', (info) => {
      assert.ok(/use PowerShell/i.test(finder.errorLog[0]), 'expect error')
      assert.ok(!info, 'no data')
    })
  })

  it('empty JSON from PowerShell', function () {
    const finder = new TestVisualStudioFinder(semverV1, null, null)

    finder.parseData(null, '[]', '', (info) => {
      assert.ok(/find .* Visual Studio/i.test(finder.errorLog[0]), 'expect error')
      assert.ok(!info, 'no data')
    })
  })

  it('future version', function () {
    const finder = new TestVisualStudioFinder(semverV1, null, null)

    finder.parseData(null, JSON.stringify([{
      packages: [
        'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
        'Microsoft.VisualStudio.Component.Windows10SDK.17763',
        'Microsoft.VisualStudio.VC.MSBuild.Base'
      ],
      path: 'C:\\VS',
      version: '9999.9999.9999.9999'
    }]), '', (info) => {
      assert.ok(/unknown version/i.test(finder.errorLog[0]), 'expect error')
      assert.ok(/find .* Visual Studio/i.test(finder.errorLog[1]), 'expect error')
      assert.ok(!info, 'no data')
    })
  })

  it('single unusable VS2017', function () {
    const finder = new TestVisualStudioFinder(semverV1, null, null)

    const file = path.join(__dirname, 'fixtures', 'VS_2017_Unusable.txt')
    const data = fs.readFileSync(file)
    finder.parseData(null, data, '', (info) => {
      assert.ok(/checking/i.test(finder.errorLog[0]), 'expect error')
      assert.ok(/find .* Visual Studio/i.test(finder.errorLog[2]), 'expect error')
      assert.ok(!info, 'no data')
    })
  })

  it('minimal VS2017 Build Tools', function () {
    const finder = new TestVisualStudioFinder(semverV1, null, (err, info) => {
      assert.strictEqual(err, null)
      assert.deepStrictEqual(info, {
        msBuild: 'C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\' +
          'BuildTools\\MSBuild\\15.0\\Bin\\MSBuild.exe',
        path:
          'C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\BuildTools',
        sdk: '10.0.17134.0',
        toolset: 'v141',
        version: '15.9.28307.665',
        versionMajor: 15,
        versionMinor: 9,
        versionYear: 2017
      })
    })

    poison(finder, 'regSearchKeys')
    finder.findVisualStudio2017OrNewer = (cb) => {
      const file = path.join(__dirname, 'fixtures',
        'VS_2017_BuildTools_minimal.txt')
      const data = fs.readFileSync(file)
      finder.parseData(null, data, '', cb)
    }
    finder.findVisualStudio()
  })

  it('VS2017 Community with C++ workload', function () {
    const finder = new TestVisualStudioFinder(semverV1, null, (err, info) => {
      assert.strictEqual(err, null)
      assert.deepStrictEqual(info, {
        msBuild: 'C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\' +
          'Community\\MSBuild\\15.0\\Bin\\MSBuild.exe',
        path:
          'C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community',
        sdk: '10.0.17763.0',
        toolset: 'v141',
        version: '15.9.28307.665',
        versionMajor: 15,
        versionMinor: 9,
        versionYear: 2017
      })
    })

    poison(finder, 'regSearchKeys')
    finder.findVisualStudio2017OrNewer = (cb) => {
      const file = path.join(__dirname, 'fixtures',
        'VS_2017_Community_workload.txt')
      const data = fs.readFileSync(file)
      finder.parseData(null, data, '', cb)
    }
    finder.findVisualStudio()
  })

  it('VS2017 Express', function () {
    const finder = new TestVisualStudioFinder(semverV1, null, (err, info) => {
      assert.strictEqual(err, null)
      assert.deepStrictEqual(info, {
        msBuild: 'C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\' +
          'WDExpress\\MSBuild\\15.0\\Bin\\MSBuild.exe',
        path:
          'C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\WDExpress',
        sdk: '10.0.17763.0',
        toolset: 'v141',
        version: '15.9.28307.858',
        versionMajor: 15,
        versionMinor: 9,
        versionYear: 2017
      })
    })

    poison(finder, 'regSearchKeys')
    finder.findVisualStudio2017OrNewer = (cb) => {
      const file = path.join(__dirname, 'fixtures', 'VS_2017_Express.txt')
      const data = fs.readFileSync(file)
      finder.parseData(null, data, '', cb)
    }
    finder.findVisualStudio()
  })

  it('VS2019 Preview with C++ workload', function () {
    const finder = new TestVisualStudioFinder(semverV1, null, (err, info) => {
      assert.strictEqual(err, null)
      assert.deepStrictEqual(info, {
        msBuild: 'C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\' +
          'Preview\\MSBuild\\Current\\Bin\\MSBuild.exe',
        path:
          'C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Preview',
        sdk: '10.0.17763.0',
        toolset: 'v142',
        version: '16.0.28608.199',
        versionMajor: 16,
        versionMinor: 0,
        versionYear: 2019
      })
    })

    poison(finder, 'regSearchKeys')
    finder.findVisualStudio2017OrNewer = (cb) => {
      const file = path.join(__dirname, 'fixtures',
        'VS_2019_Preview.txt')
      const data = fs.readFileSync(file)
      finder.parseData(null, data, '', cb)
    }
    finder.findVisualStudio()
  })

  it('minimal VS2019 Build Tools', function () {
    const finder = new TestVisualStudioFinder(semverV1, null, (err, info) => {
      assert.strictEqual(err, null)
      assert.deepStrictEqual(info, {
        msBuild: 'C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\' +
          'BuildTools\\MSBuild\\Current\\Bin\\MSBuild.exe',
        path:
          'C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\BuildTools',
        sdk: '10.0.17134.0',
        toolset: 'v142',
        version: '16.1.28922.388',
        versionMajor: 16,
        versionMinor: 1,
        versionYear: 2019
      })
    })

    poison(finder, 'regSearchKeys')
    finder.findVisualStudio2017OrNewer = (cb) => {
      const file = path.join(__dirname, 'fixtures',
        'VS_2019_BuildTools_minimal.txt')
      const data = fs.readFileSync(file)
      finder.parseData(null, data, '', cb)
    }
    finder.findVisualStudio()
  })

  it('VS2019 Community with C++ workload', function () {
    const finder = new TestVisualStudioFinder(semverV1, null, (err, info) => {
      assert.strictEqual(err, null)
      assert.deepStrictEqual(info, {
        msBuild: 'C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\' +
          'Community\\MSBuild\\Current\\Bin\\MSBuild.exe',
        path:
          'C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community',
        sdk: '10.0.17763.0',
        toolset: 'v142',
        version: '16.1.28922.388',
        versionMajor: 16,
        versionMinor: 1,
        versionYear: 2019
      })
    })

    poison(finder, 'regSearchKeys')
    finder.findVisualStudio2017OrNewer = (cb) => {
      const file = path.join(__dirname, 'fixtures',
        'VS_2019_Community_workload.txt')
      const data = fs.readFileSync(file)
      finder.parseData(null, data, '', cb)
    }
    finder.findVisualStudio()
  })

  it('VS2022 Preview with C++ workload', function () {
    const msBuildPath = process.arch === 'arm64'
      ? 'C:\\Program Files\\Microsoft Visual Studio\\2022\\' +
        'Community\\MSBuild\\Current\\Bin\\arm64\\MSBuild.exe'
      : 'C:\\Program Files\\Microsoft Visual Studio\\2022\\' +
        'Community\\MSBuild\\Current\\Bin\\MSBuild.exe'

    const finder = new TestVisualStudioFinder(semverV1, null, (err, info) => {
      assert.strictEqual(err, null)
      assert.deepStrictEqual(info, {
        msBuild: msBuildPath,
        path:
          'C:\\Program Files\\Microsoft Visual Studio\\2022\\Community',
        sdk: '10.0.22621.0',
        toolset: 'v143',
        version: '17.4.33213.308',
        versionMajor: 17,
        versionMinor: 4,
        versionYear: 2022
      })
    })

    poison(finder, 'regSearchKeys')
    finder.msBuildPathExists = (path) => {
      return true
    }
    finder.findVisualStudio2017OrNewer = (cb) => {
      const file = path.join(__dirname, 'fixtures',
        'VS_2022_Community_workload.txt')
      const data = fs.readFileSync(file)
      finder.parseData(null, data, '', cb)
    }
    finder.findVisualStudio()
  })

  function allVsVersions (finder) {
    finder.findVisualStudio2017OrNewer = (cb) => {
      const data0 = JSON.parse(fs.readFileSync(path.join(__dirname, 'fixtures',
        'VS_2017_Unusable.txt')))
      const data1 = JSON.parse(fs.readFileSync(path.join(__dirname, 'fixtures',
        'VS_2017_BuildTools_minimal.txt')))
      const data2 = JSON.parse(fs.readFileSync(path.join(__dirname, 'fixtures',
        'VS_2017_Community_workload.txt')))
      const data3 = JSON.parse(fs.readFileSync(path.join(__dirname, 'fixtures',
        'VS_2017_Express.txt')))
      const data4 = JSON.parse(fs.readFileSync(path.join(__dirname, 'fixtures',
        'VS_2019_Preview.txt')))
      const data5 = JSON.parse(fs.readFileSync(path.join(__dirname, 'fixtures',
        'VS_2019_BuildTools_minimal.txt')))
      const data6 = JSON.parse(fs.readFileSync(path.join(__dirname, 'fixtures',
        'VS_2019_Community_workload.txt')))
      const data7 = JSON.parse(fs.readFileSync(path.join(__dirname, 'fixtures',
        'VS_2022_Community_workload.txt')))
      const data = JSON.stringify(data0.concat(data1, data2, data3, data4,
        data5, data6, data7))
      finder.parseData(null, data, '', cb)
    }
    finder.regSearchKeys = (keys, value, addOpts, cb) => {
      for (var i = 0; i < keys.length; ++i) {
        const fullName = `${keys[i]}\\${value}`
        switch (fullName) {
          case 'HKLM\\Software\\Microsoft\\VisualStudio\\SxS\\VC7\\14.0':
          case 'HKLM\\Software\\Microsoft\\VisualStudio\\SxS\\VC7\\12.0':
            continue
          case 'HKLM\\Software\\Wow6432Node\\Microsoft\\VisualStudio\\SxS\\VC7\\12.0':
            return cb(null, 'C:\\VS2013\\VC\\')
          case 'HKLM\\Software\\Microsoft\\MSBuild\\ToolsVersions\\12.0\\MSBuildToolsPath':
            return cb(null, 'C:\\MSBuild12\\')
          case 'HKLM\\Software\\Wow6432Node\\Microsoft\\VisualStudio\\SxS\\VC7\\14.0':
            return cb(null, 'C:\\VS2015\\VC\\')
          case 'HKLM\\Software\\Microsoft\\MSBuild\\ToolsVersions\\14.0\\MSBuildToolsPath':
            return cb(null, 'C:\\MSBuild14\\')
          default:
            assert.fail(`unexpected search for registry value ${fullName}`)
        }
      }
      return cb(new Error())
    }
  }

  it('fail when looking for invalid path', function () {
    const finder = new TestVisualStudioFinder(semverV1, 'AABB', (err, info) => {
      assert.ok(/find .* Visual Studio/i.test(err), 'expect error')
      assert.ok(!info, 'no data')
    })

    allVsVersions(finder)
    finder.findVisualStudio()
  })

  it('look for VS2013 by version number', function () {
    const finder = new TestVisualStudioFinder(semverV1, '2013', (err, info) => {
      assert.strictEqual(err, null)
      assert.deepStrictEqual(info.versionYear, 2013)
    })

    allVsVersions(finder)
    finder.findVisualStudio()
  })

  it('look for VS2013 by installation path', function () {
    const finder = new TestVisualStudioFinder(semverV1, 'C:\\VS2013',
      (err, info) => {
        assert.strictEqual(err, null)
        assert.deepStrictEqual(info.path, 'C:\\VS2013')
      })

    allVsVersions(finder)
    finder.findVisualStudio()
  })

  it('look for VS2015 by version number', function () {
    const finder = new TestVisualStudioFinder(semverV1, '2015', (err, info) => {
      assert.strictEqual(err, null)
      assert.deepStrictEqual(info.versionYear, 2015)
    })

    allVsVersions(finder)
    finder.findVisualStudio()
  })

  it('look for VS2015 by installation path', function () {
    const finder = new TestVisualStudioFinder(semverV1, 'C:\\VS2015',
      (err, info) => {
        assert.strictEqual(err, null)
        assert.deepStrictEqual(info.path, 'C:\\VS2015')
      })

    allVsVersions(finder)
    finder.findVisualStudio()
  })

  it('look for VS2017 by version number', function () {
    const finder = new TestVisualStudioFinder(semverV1, '2017', (err, info) => {
      assert.strictEqual(err, null)
      assert.deepStrictEqual(info.versionYear, 2017)
    })

    allVsVersions(finder)
    finder.findVisualStudio()
  })

  it('look for VS2017 by installation path', function () {
    const finder = new TestVisualStudioFinder(semverV1,
      'C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community',
      (err, info) => {
        assert.strictEqual(err, null)
        assert.deepStrictEqual(info.path,
          'C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community')
      })

    allVsVersions(finder)
    finder.findVisualStudio()
  })

  it('look for VS2019 by version number', function () {
    const finder = new TestVisualStudioFinder(semverV1, '2019', (err, info) => {
      assert.strictEqual(err, null)
      assert.deepStrictEqual(info.versionYear, 2019)
    })

    allVsVersions(finder)
    finder.findVisualStudio()
  })

  it('look for VS2019 by installation path', function () {
    const finder = new TestVisualStudioFinder(semverV1,
      'C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\BuildTools',
      (err, info) => {
        assert.strictEqual(err, null)
        assert.deepStrictEqual(info.path,
          'C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\BuildTools')
      })

    allVsVersions(finder)
    finder.findVisualStudio()
  })

  it('look for VS2022 by version number', function () {
    const finder = new TestVisualStudioFinder(semverV1, '2022', (err, info) => {
      assert.strictEqual(err, null)
      assert.deepStrictEqual(info.versionYear, 2022)
    })

    finder.msBuildPathExists = (path) => {
      return true
    }

    allVsVersions(finder)
    finder.findVisualStudio()
  })

  it('msvs_version match should be case insensitive', function () {
    const finder = new TestVisualStudioFinder(semverV1,
      'c:\\program files (x86)\\microsoft visual studio\\2019\\BUILDTOOLS',
      (err, info) => {
        assert.strictEqual(err, null)
        assert.deepStrictEqual(info.path,
          'C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\BuildTools')
      })

    allVsVersions(finder)
    finder.findVisualStudio()
  })

  it('latest version should be found by default', function () {
    const finder = new TestVisualStudioFinder(semverV1, null, (err, info) => {
      assert.strictEqual(err, null)
      assert.deepStrictEqual(info.versionYear, 2022)
    })

    finder.msBuildPathExists = (path) => {
      return true
    }

    allVsVersions(finder)
    finder.findVisualStudio()
  })

  it('run on a usable VS Command Prompt', function () {
    process.env.VCINSTALLDIR = 'C:\\VS2015\\VC'
    // VSINSTALLDIR is not defined on Visual C++ Build Tools 2015
    delete process.env.VSINSTALLDIR

    const finder = new TestVisualStudioFinder(semverV1, null, (err, info) => {
      assert.strictEqual(err, null)
      assert.deepStrictEqual(info.path, 'C:\\VS2015')
    })

    allVsVersions(finder)
    finder.findVisualStudio()
  })

  it('VCINSTALLDIR match should be case insensitive', function () {
    process.env.VCINSTALLDIR =
      'c:\\program files (x86)\\microsoft visual studio\\2019\\BUILDTOOLS\\VC'

    const finder = new TestVisualStudioFinder(semverV1, null, (err, info) => {
      assert.strictEqual(err, null)
      assert.deepStrictEqual(info.path,
        'C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\BuildTools')
    })

    allVsVersions(finder)
    finder.findVisualStudio()
  })

  it('run on a unusable VS Command Prompt', function () {
    process.env.VCINSTALLDIR =
      'C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\BuildToolsUnusable\\VC'

    const finder = new TestVisualStudioFinder(semverV1, null, (err, info) => {
      assert.ok(/find .* Visual Studio/i.test(err), 'expect error')
      assert.ok(!info, 'no data')
    })

    allVsVersions(finder)
    finder.findVisualStudio()
  })

  it('run on a VS Command Prompt with matching msvs_version', function () {
    process.env.VCINSTALLDIR = 'C:\\VS2015\\VC'

    const finder = new TestVisualStudioFinder(semverV1, 'C:\\VS2015',
      (err, info) => {
        assert.strictEqual(err, null)
        assert.deepStrictEqual(info.path, 'C:\\VS2015')
      })

    allVsVersions(finder)
    finder.findVisualStudio()
  })

  it('run on a VS Command Prompt with mismatched msvs_version', function () {
    process.env.VCINSTALLDIR =
      'C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\BuildTools\\VC'

    const finder = new TestVisualStudioFinder(semverV1, 'C:\\VS2015',
      (err, info) => {
        assert.ok(/find .* Visual Studio/i.test(err), 'expect error')
        assert.ok(!info, 'no data')
      })

    allVsVersions(finder)
    finder.findVisualStudio()
  })
})
