/* global ___NYC_SELF_COVERAGE___ */

const path = require('path')
const fs = require('fs')
const mkdirp = require('mkdirp')
const onExit = require('signal-exit')

onExit(function () {
  var coverage = global.___NYC_SELF_COVERAGE___
  if (typeof ___NYC_SELF_COVERAGE___ === 'object') coverage = ___NYC_SELF_COVERAGE___
  if (!coverage) return

  var selfCoverageDir = path.join(__dirname, '../.self_coverage')
  mkdirp.sync(selfCoverageDir)
  fs.writeFileSync(
    path.join(selfCoverageDir, process.pid + '.json'),
    JSON.stringify(coverage),
    'utf-8'
  )
})
