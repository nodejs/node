'use strict'

const BB = require('bluebird')

const extractionWorker = require('./worker.js')
const figgyPudding = require('figgy-pudding')
const npa = require('npm-package-arg')
const WORKER_PATH = require.resolve('./worker.js')
let workerFarm

// Broken for now, cause too many issues on some systems.
const ENABLE_WORKERS = false

const ExtractOpts = figgyPudding({
  log: {}
})

module.exports = {
  startWorkers () {
    if (ENABLE_WORKERS) {
      if (!workerFarm) { workerFarm = require('worker-farm') }
      this._workers = workerFarm({
        maxConcurrentCallsPerWorker: 20,
        maxRetries: 1
      }, WORKER_PATH)
    }
  },

  stopWorkers () {
    if (ENABLE_WORKERS) {
      if (!workerFarm) { workerFarm = require('worker-farm') }
      workerFarm.end(this._workers)
    }
  },

  child (name, child, childPath, opts) {
    opts = ExtractOpts(opts)
    const spec = npa.resolve(name, child.version)
    let childOpts = opts.concat({
      integrity: child.integrity,
      resolved: child.resolved
    })
    const args = [spec, childPath, childOpts]
    return BB.fromNode((cb) => {
      let launcher = extractionWorker
      let msg = args
      const spec = typeof args[0] === 'string' ? npa(args[0]) : args[0]
      if (ENABLE_WORKERS && (spec.registry || spec.type === 'remote')) {
        if (!workerFarm) { workerFarm = require('worker-farm') }
        // We can't serialize these options
        childOpts = childOpts.concat({
          log: null,
          dirPacker: null
        })
        // workers will run things in parallel!
        launcher = this._workers
        try {
          msg = JSON.stringify(msg)
        } catch (e) {
          return cb(e)
        }
      }
      launcher(msg, cb)
    })
  }
}
