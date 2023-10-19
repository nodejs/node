'use strict'

const { describe, it } = require('mocha')
const assert = require('assert')
const path = require('path')
const fs = require('graceful-fs')
const childProcess = require('child_process')
const os = require('os')
const addonPath = path.resolve(__dirname, 'node_modules', 'hello_world')
const nodeGyp = path.resolve(__dirname, '..', 'bin', 'node-gyp.js')
const execFileSync = childProcess.execFileSync || require('./process-exec-sync')
const execFile = childProcess.execFile

function runHello (hostProcess) {
  if (!hostProcess) {
    hostProcess = process.execPath
  }
  var testCode = "console.log(require('hello_world').hello())"
  return execFileSync(hostProcess, ['-e', testCode], { cwd: __dirname }).toString()
}

function getEncoding () {
  var code = 'import locale;print(locale.getdefaultlocale()[1])'
  return execFileSync('python', ['-c', code]).toString().trim()
}

function checkCharmapValid () {
  var data
  try {
    data = execFileSync('python', ['fixtures/test-charmap.py'],
      { cwd: __dirname })
  } catch (err) {
    return false
  }
  var lines = data.toString().trim().split('\n')
  return lines.pop() === 'True'
}

describe('addon', function () {
  this.timeout(300000)

  it('build simple addon', function (done) {
    // Set the loglevel otherwise the output disappears when run via 'npm test'
    var cmd = [nodeGyp, 'rebuild', '-C', addonPath, '--loglevel=verbose']
    var proc = execFile(process.execPath, cmd, function (err, stdout, stderr) {
      var logLines = stderr.toString().trim().split(/\r?\n/)
      var lastLine = logLines[logLines.length - 1]
      assert.strictEqual(err, null)
      assert.strictEqual(lastLine, 'gyp info ok', 'should end in ok')
      assert.strictEqual(runHello().trim(), 'world')
      done()
    })
    proc.stdout.setEncoding('utf-8')
    proc.stderr.setEncoding('utf-8')
  })

  it('build simple addon in path with non-ascii characters', function (done) {
    if (!checkCharmapValid()) {
      return this.skip('python console app can\'t encode non-ascii character.')
    }

    var testDirNames = {
      cp936: '文件夹',
      cp1252: 'Latīna',
      cp932: 'フォルダ'
    }
    // Select non-ascii characters by current encoding
    var testDirName = testDirNames[getEncoding()]
    // If encoding is UTF-8 or other then no need to test
    if (!testDirName) {
      return this.skip('no need to test')
    }

    this.timeout(300000)

    var data
    var configPath = path.join(addonPath, 'build', 'config.gypi')
    try {
      data = fs.readFileSync(configPath, 'utf8')
    } catch (err) {
      assert.fail(err)
      return
    }
    var config = JSON.parse(data.replace(/#.+\n/, ''))
    var nodeDir = config.variables.nodedir
    var testNodeDir = path.join(addonPath, testDirName)
    // Create symbol link to path with non-ascii characters
    try {
      fs.symlinkSync(nodeDir, testNodeDir, 'dir')
    } catch (err) {
      switch (err.code) {
        case 'EEXIST': break
        case 'EPERM':
          assert.fail(err, null, 'Please try to running console as an administrator')
          return
        default:
          assert.fail(err)
          return
      }
    }

    var cmd = [
      nodeGyp,
      'rebuild',
      '-C',
      addonPath,
      '--loglevel=verbose',
      '-nodedir=' + testNodeDir
    ]
    var proc = execFile(process.execPath, cmd, function (err, stdout, stderr) {
      try {
        fs.unlink(testNodeDir)
      } catch (err) {
        assert.fail(err)
      }

      var logLines = stderr.toString().trim().split(/\r?\n/)
      var lastLine = logLines[logLines.length - 1]
      assert.strictEqual(err, null)
      assert.strictEqual(lastLine, 'gyp info ok', 'should end in ok')
      assert.strictEqual(runHello().trim(), 'world')
      done()
    })
    proc.stdout.setEncoding('utf-8')
    proc.stderr.setEncoding('utf-8')
  })

  it('addon works with renamed host executable', function (done) {
    // No `fs.copyFileSync` before node8.
    if (process.version.substr(1).split('.')[0] < 8) {
      return this.skip('skipping test for old node version')
    }

    this.timeout(300000)

    var notNodePath = path.join(os.tmpdir(), 'notnode' + path.extname(process.execPath))
    fs.copyFileSync(process.execPath, notNodePath)

    var cmd = [nodeGyp, 'rebuild', '-C', addonPath, '--loglevel=verbose']
    var proc = execFile(process.execPath, cmd, function (err, stdout, stderr) {
      var logLines = stderr.toString().trim().split(/\r?\n/)
      var lastLine = logLines[logLines.length - 1]
      assert.strictEqual(err, null)
      assert.strictEqual(lastLine, 'gyp info ok', 'should end in ok')
      assert.strictEqual(runHello(notNodePath).trim(), 'world')
      fs.unlinkSync(notNodePath)
      done()
    })
    proc.stdout.setEncoding('utf-8')
    proc.stderr.setEncoding('utf-8')
  })
})
