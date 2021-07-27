'use strict'

module.exports = spawn

const _spawn = require('child_process').spawn
const EventEmitter = require('events').EventEmitter

let progressEnabled
let running = 0

function startRunning (log) {
  if (progressEnabled == null) progressEnabled = log.progressEnabled
  if (progressEnabled) log.disableProgress()
  ++running
}

function stopRunning (log) {
  --running
  if (progressEnabled && running === 0) log.enableProgress()
}

function willCmdOutput (stdio) {
  if (stdio === 'inherit') return true
  if (!Array.isArray(stdio)) return false
  for (let fh = 1; fh <= 2; ++fh) {
    if (stdio[fh] === 'inherit') return true
    if (stdio[fh] === 1 || stdio[fh] === 2) return true
  }
  return false
}

function spawn (cmd, args, options, log) {
  const cmdWillOutput = willCmdOutput(options && options.stdio)

  if (cmdWillOutput) startRunning(log)
  const raw = _spawn(cmd, args, options)
  const cooked = new EventEmitter()

  raw.on('error', function (er) {
    if (cmdWillOutput) stopRunning(log)
    er.file = cmd
    cooked.emit('error', er)
  }).on('close', function (code, signal) {
    if (cmdWillOutput) stopRunning(log)
    // Create ENOENT error because Node.js v8.0 will not emit
    // an `error` event if the command could not be found.
    if (code === 127) {
      const er = new Error('spawn ENOENT')
      er.code = 'ENOENT'
      er.errno = 'ENOENT'
      er.syscall = 'spawn'
      er.file = cmd
      cooked.emit('error', er)
    } else {
      cooked.emit('close', code, signal)
    }
  })

  cooked.stdin = raw.stdin
  cooked.stdout = raw.stdout
  cooked.stderr = raw.stderr
  cooked.kill = function (sig) { return raw.kill(sig) }

  return cooked
}
