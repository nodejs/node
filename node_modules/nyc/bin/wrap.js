var sw = require('spawn-wrap')
var NYC
try {
  NYC = require('../index.covered.js')
} catch (e) {
  NYC = require('../index.js')
}

var parentPid = process.env.NYC_PARENT_PID || '0'
process.env.NYC_PARENT_PID = process.pid

var config = {}
if (process.env.NYC_CONFIG) config = JSON.parse(process.env.NYC_CONFIG)
config.isChildProcess = true

config._processInfo = {
  ppid: parentPid,
  root: process.env.NYC_ROOT_ID
}

;(new NYC(config)).wrap()

sw.runMain()
