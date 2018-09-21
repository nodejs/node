'use strict'

const BB = require('bluebird')

const npa = require('npm-package-arg')
const workerFarm = require('worker-farm')

const extractionWorker = require('./worker.js')
const WORKER_PATH = require.resolve('./worker.js')

module.exports = {
  startWorkers () {
    this._workers = workerFarm({
      maxConcurrentCallsPerWorker: 20,
      maxRetries: 1
    }, WORKER_PATH)
  },

  stopWorkers () {
    workerFarm.end(this._workers)
  },

  child (name, child, childPath, config, opts) {
    const spec = npa.resolve(name, child.version)
    const additionalToPacoteOpts = {}
    if (typeof opts.dirPacker !== 'undefined') {
      additionalToPacoteOpts.dirPacker = opts.dirPacker
    }
    const childOpts = config.toPacote(Object.assign({
      integrity: child.integrity,
      resolved: child.resolved
    }, additionalToPacoteOpts))
    const args = [spec, childPath, childOpts]
    return BB.fromNode((cb) => {
      let launcher = extractionWorker
      let msg = args
      const spec = typeof args[0] === 'string' ? npa(args[0]) : args[0]
      childOpts.loglevel = opts.log.level
      if (spec.registry || spec.type === 'remote') {
        // We can't serialize these options
        childOpts.config = null
        childOpts.log = null
        childOpts.dirPacker = null
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
