const { spawn } = require('child_process')
const inferOwner = require('infer-owner')

const isPipe = (stdio = 'pipe', fd) =>
  stdio === 'pipe' || stdio === null ? true
  : Array.isArray(stdio) ? isPipe(stdio[fd], fd)
  : false

// 'extra' object is for decorating the error a bit more
const promiseSpawn = (cmd, args, opts = {}, extra = {}) => {
  const cwd = opts.cwd || process.cwd()
  const isRoot = process.getuid && process.getuid() === 0
  const { uid, gid } = isRoot ? inferOwner.sync(cwd) : {}
  return promiseSpawnUid(cmd, args, {
    ...opts,
    cwd,
    uid,
    gid,
  }, extra)
}

const stdioResult = (stdout, stderr, { stdioString, stdio }) =>
  stdioString ? {
    stdout: isPipe(stdio, 1) ? Buffer.concat(stdout).toString() : null,
    stderr: isPipe(stdio, 2) ? Buffer.concat(stderr).toString() : null,
  }
  : {
    stdout: isPipe(stdio, 1) ? Buffer.concat(stdout) : null,
    stderr: isPipe(stdio, 2) ? Buffer.concat(stderr) : null,
  }

const promiseSpawnUid = (cmd, args, opts, extra) => {
  let proc
  const p = new Promise((res, rej) => {
    proc = spawn(cmd, args, opts)
    const stdout = []
    const stderr = []
    const reject = er => rej(Object.assign(er, {
      cmd,
      args,
      ...stdioResult(stdout, stderr, opts),
      ...extra,
    }))
    proc.on('error', reject)
    if (proc.stdout) {
      proc.stdout.on('data', c => stdout.push(c)).on('error', reject)
      proc.stdout.on('error', er => reject(er))
    }
    if (proc.stderr) {
      proc.stderr.on('data', c => stderr.push(c)).on('error', reject)
      proc.stderr.on('error', er => reject(er))
    }
    proc.on('close', (code, signal) => {
      const result = {
        cmd,
        args,
        code,
        signal,
        ...stdioResult(stdout, stderr, opts),
        ...extra,
      }
      if (code || signal) {
        rej(Object.assign(new Error('command failed'), result))
      } else {
        res(result)
      }
    })
  })

  p.stdin = proc.stdin
  p.process = proc
  return p
}

module.exports = promiseSpawn
