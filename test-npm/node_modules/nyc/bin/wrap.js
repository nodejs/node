var sw = require('spawn-wrap')
var NYC
try {
  NYC = require('../index.covered.js')
} catch (e) {
  NYC = require('../index.js')
}

;(new NYC({
  require: process.env.NYC_REQUIRE ? process.env.NYC_REQUIRE.split(',') : [],
  extension: process.env.NYC_EXTENSION ? process.env.NYC_EXTENSION.split(',') : [],
  exclude: process.env.NYC_EXCLUDE ? process.env.NYC_EXCLUDE.split(',') : [],
  include: process.env.NYC_INCLUDE ? process.env.NYC_INCLUDE.split(',') : [],
  enableCache: process.env.NYC_CACHE === 'enable',
  sourceMap: process.env.NYC_SOURCE_MAP === 'enable',
  instrumenter: process.env.NYC_INSTRUMENTER,
  hookRunInContext: process.env.NYC_HOOK_RUN_IN_CONTEXT === 'enable'
})).wrap()

sw.runMain()
