'use strict'
module.exports = launchSendMetrics
var fs = require('graceful-fs')
var child_process = require('child_process')

if (require.main === module) main()

function launchSendMetrics () {
  var path = require('path')
  var npm = require('../npm.js')
  try {
    if (!npm.config.get('send-metrics')) return
    var cliMetrics = path.join(npm.config.get('cache'), 'anonymous-cli-metrics.json')
    var targetRegistry = npm.config.get('metrics-registry')
    fs.statSync(cliMetrics)
    return runInBackground(__filename, [cliMetrics, targetRegistry])
  } catch (ex) {
    // if the metrics file doesn't exist, don't run
  }
}

function runInBackground (js, args, opts) {
  if (!args) args = []
  args.unshift(js)
  if (!opts) opts = {}
  opts.stdio = 'ignore'
  opts.detached = true
  var child = child_process.spawn(process.execPath, args, opts)
  child.unref()
  return child
}

function main () {
  var sendMetrics = require('./metrics.js').send
  var metricsFile = process.argv[2]
  var metricsRegistry = process.argv[3]

  sendMetrics(metricsFile, metricsRegistry)
}
