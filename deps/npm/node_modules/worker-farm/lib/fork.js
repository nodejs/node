'use strict'

const childProcess = require('child_process')
    , childModule  = require.resolve('./child/index')


function fork (forkModule) {
  // suppress --debug / --inspect flags while preserving others (like --harmony)
  let filteredArgs = process.execArgv.filter(function (v) {
        return !(/^--(debug|inspect)/).test(v)
      })
    , child        = childProcess.fork(childModule, process.argv, {
          execArgv: filteredArgs
        , env: process.env
        , cwd: process.cwd()
      })

  child.on('error', function() {
    // this *should* be picked up by onExit and the operation requeued
  })

  child.send({ module: forkModule })

  // return a send() function for this child
  return {
      send  : child.send.bind(child)
    , child : child
  }
}


module.exports = fork
