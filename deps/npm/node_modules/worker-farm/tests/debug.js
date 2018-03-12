'use strict'

const workerFarm = require('../')
    , workers    = workerFarm(require.resolve('./child'), ['args'])


workers.args(function() {
  workerFarm.end(workers)
  console.log('FINISHED')
  process.exit(0)
})
