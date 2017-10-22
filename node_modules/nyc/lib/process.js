const archy = require('archy')
const libCoverage = require('istanbul-lib-coverage')

function ProcessInfo (defaults) {
  defaults = defaults || {}

  this.pid = String(process.pid)
  this.argv = process.argv
  this.execArgv = process.execArgv
  this.cwd = process.cwd()
  this.time = Date.now()
  this.ppid = null
  this.root = null
  this.coverageFilename = null
  this.nodes = [] // list of children, filled by buildProcessTree()

  this._coverageMap = null

  for (var key in defaults) {
    this[key] = defaults[key]
  }
}

Object.defineProperty(ProcessInfo.prototype, 'label', {
  get: function () {
    if (this._label) {
      return this._label
    }

    var covInfo = ''
    if (this._coverageMap) {
      covInfo = '\n  ' + this._coverageMap.getCoverageSummary().lines.pct + ' % Lines'
    }
    return this.argv.join(' ') + covInfo
  }
})

ProcessInfo.buildProcessTree = function (infos) {
  var treeRoot = new ProcessInfo({ _label: 'nyc' })
  var nodes = { }

  infos = infos.sort(function (a, b) {
    return a.time - b.time
  })

  infos.forEach(function (p) {
    nodes[p.root + ':' + p.pid] = p
  })

  infos.forEach(function (p) {
    if (!p.ppid) {
      return
    }

    var parent = nodes[p.root + ':' + p.ppid]
    if (!parent) {
      parent = treeRoot
    }

    parent.nodes.push(p)
  })

  return treeRoot
}

ProcessInfo.prototype.getCoverageMap = function (merger) {
  if (this._coverageMap) {
    return this._coverageMap
  }

  var childMaps = this.nodes.map(function (child) {
    return child.getCoverageMap(merger)
  })

  this._coverageMap = merger([this.coverageFilename], childMaps)

  return this._coverageMap
}

ProcessInfo.prototype.render = function (nyc) {
  this.getCoverageMap(function (filenames, maps) {
    var map = libCoverage.createCoverageMap({})

    nyc.loadReports(filenames).forEach(function (report) {
      map.merge(report)
    })

    maps.forEach(function (otherMap) {
      map.merge(otherMap)
    })

    return map
  })

  return archy(this)
}

module.exports = ProcessInfo
