/* global __coverage__ */

const arrify = require('arrify')
const cachingTransform = require('caching-transform')
const debugLog = require('debug-log')('nyc')
const findCacheDir = require('find-cache-dir')
const fs = require('fs')
const glob = require('glob')
const Hash = require('./lib/hash')
const js = require('default-require-extensions/js')
const libCoverage = require('istanbul-lib-coverage')
const libHook = require('istanbul-lib-hook')
const libReport = require('istanbul-lib-report')
const md5hex = require('md5-hex')
const mkdirp = require('mkdirp')
const Module = require('module')
const onExit = require('signal-exit')
const path = require('path')
const reports = require('istanbul-reports')
const resolveFrom = require('resolve-from')
const rimraf = require('rimraf')
const SourceMaps = require('./lib/source-maps')
const testExclude = require('test-exclude')

var ProcessInfo
try {
  ProcessInfo = require('./lib/process.covered.js')
} catch (e) {
  /* istanbul ignore next */
  ProcessInfo = require('./lib/process.js')
}

/* istanbul ignore next */
if (/index\.covered\.js$/.test(__filename)) {
  require('./lib/self-coverage-helper')
}

function NYC (config) {
  config = config || {}
  this.config = config

  this.subprocessBin = config.subprocessBin || path.resolve(__dirname, './bin/nyc.js')
  this._tempDirectory = config.tempDirectory || './.nyc_output'
  this._instrumenterLib = require(config.instrumenter || './lib/instrumenters/istanbul')
  this._reportDir = config.reportDir || 'coverage'
  this._sourceMap = typeof config.sourceMap === 'boolean' ? config.sourceMap : true
  this._showProcessTree = config.showProcessTree || false
  this._eagerInstantiation = config.eager || false
  this.cwd = config.cwd || process.cwd()
  this.reporter = arrify(config.reporter || 'text')

  this.cacheDirectory = config.cacheDir || findCacheDir({name: 'nyc', cwd: this.cwd})
  this.cache = Boolean(this.cacheDirectory && config.cache)

  this.exclude = testExclude({
    cwd: this.cwd,
    include: config.include,
    exclude: config.exclude
  })

  this.sourceMaps = new SourceMaps({
    cache: this.cache,
    cacheDirectory: this.cacheDirectory
  })

  // require extensions can be provided as config in package.json.
  this.require = arrify(config.require)

  this.extensions = arrify(config.extension).concat('.js').map(function (ext) {
    return ext.toLowerCase()
  }).filter(function (item, pos, arr) {
    // avoid duplicate extensions
    return arr.indexOf(item) === pos
  })

  this.transforms = this.extensions.reduce(function (transforms, ext) {
    transforms[ext] = this._createTransform(ext)
    return transforms
  }.bind(this), {})

  this.hookRunInContext = config.hookRunInContext
  this.fakeRequire = null

  this.processInfo = new ProcessInfo(config && config._processInfo)
  this.rootId = this.processInfo.root || this.generateUniqueID()

  this.hashCache = {}
}

NYC.prototype._createTransform = function (ext) {
  var _this = this
  var opts = {
    salt: Hash.salt,
    hash: function (code, metadata, salt) {
      var hash = Hash(code, metadata.filename)
      _this.hashCache[metadata.filename] = hash
      return hash
    },
    cacheDir: this.cacheDirectory,
    // when running --all we should not load source-file from
    // cache, we want to instead return the fake source.
    disableCache: this._disableCachingTransform(),
    ext: ext
  }
  if (this._eagerInstantiation) {
    opts.transform = this._transformFactory(this.cacheDirectory)
  } else {
    opts.factory = this._transformFactory.bind(this)
  }
  return cachingTransform(opts)
}

NYC.prototype._disableCachingTransform = function () {
  return !(this.cache && this.config.isChildProcess)
}

NYC.prototype._loadAdditionalModules = function () {
  var _this = this
  this.require.forEach(function (r) {
    // first attempt to require the module relative to
    // the directory being instrumented.
    var p = resolveFrom(_this.cwd, r)
    if (p) {
      require(p)
      return
    }
    // now try other locations, .e.g, the nyc node_modules folder.
    require(r)
  })
}

NYC.prototype.instrumenter = function () {
  return this._instrumenter || (this._instrumenter = this._createInstrumenter())
}

NYC.prototype._createInstrumenter = function () {
  return this._instrumenterLib(this.cwd, {
    produceSourceMap: this.config.produceSourceMap
  })
}

NYC.prototype.addFile = function (filename) {
  var relFile = path.relative(this.cwd, filename)
  var source = this._readTranspiledSource(path.resolve(this.cwd, filename))
  var instrumentedSource = this._maybeInstrumentSource(source, filename, relFile)

  return {
    instrument: !!instrumentedSource,
    relFile: relFile,
    content: instrumentedSource || source
  }
}

NYC.prototype._readTranspiledSource = function (filePath) {
  var source = null
  var ext = path.extname(filePath)
  if (typeof Module._extensions[ext] === 'undefined') {
    ext = '.js'
  }
  Module._extensions[ext]({
    _compile: function (content, filename) {
      source = content
    }
  }, filePath)
  return source
}

NYC.prototype.addAllFiles = function () {
  var _this = this

  this._loadAdditionalModules()

  this.fakeRequire = true
  this.walkAllFiles(this.cwd, function (filename) {
    filename = path.resolve(_this.cwd, filename)
    _this.addFile(filename)
    var coverage = coverageFinder()
    var lastCoverage = _this.instrumenter().lastFileCoverage()
    if (lastCoverage) {
      filename = lastCoverage.path
    }
    if (lastCoverage && _this.exclude.shouldInstrument(filename)) {
      coverage[filename] = lastCoverage
    }
  })
  this.fakeRequire = false

  this.writeCoverageFile()
}

NYC.prototype.instrumentAllFiles = function (input, output, cb) {
  var _this = this
  var inputDir = '.' + path.sep
  var visitor = function (filename) {
    var ext
    var transform
    var inFile = path.resolve(inputDir, filename)
    var code = fs.readFileSync(inFile, 'utf-8')

    for (ext in _this.transforms) {
      if (filename.toLowerCase().substr(-ext.length) === ext) {
        transform = _this.transforms[ext]
        break
      }
    }

    if (transform) {
      code = transform(code, {filename: filename, relFile: inFile})
    }

    if (!output) {
      console.log(code)
    } else {
      var outFile = path.resolve(output, filename)
      mkdirp.sync(path.dirname(outFile))
      fs.writeFileSync(outFile, code, 'utf-8')
    }
  }

  this._loadAdditionalModules()

  try {
    var stats = fs.lstatSync(input)
    if (stats.isDirectory()) {
      inputDir = input
      this.walkAllFiles(input, visitor)
    } else {
      visitor(input)
    }
  } catch (err) {
    return cb(err)
  }
}

NYC.prototype.walkAllFiles = function (dir, visitor) {
  var pattern = null
  if (this.extensions.length === 1) {
    pattern = '**/*' + this.extensions[0]
  } else {
    pattern = '**/*{' + this.extensions.join() + '}'
  }

  glob.sync(pattern, {cwd: dir, nodir: true, ignore: this.exclude.exclude}).forEach(function (filename) {
    visitor(filename)
  })
}

NYC.prototype._maybeInstrumentSource = function (code, filename, relFile) {
  var instrument = this.exclude.shouldInstrument(filename, relFile)
  if (!instrument) {
    return null
  }

  var ext, transform
  for (ext in this.transforms) {
    if (filename.toLowerCase().substr(-ext.length) === ext) {
      transform = this.transforms[ext]
      break
    }
  }

  return transform ? transform(code, {filename: filename, relFile: relFile}) : null
}

NYC.prototype._transformFactory = function (cacheDir) {
  var _this = this
  var instrumenter = this.instrumenter()
  var instrumented

  return function (code, metadata, hash) {
    var filename = metadata.filename
    var sourceMap = null

    if (_this._sourceMap) sourceMap = _this.sourceMaps.extractAndRegister(code, filename, hash)

    try {
      instrumented = instrumenter.instrumentSync(code, filename, sourceMap)
    } catch (e) {
      // don't fail external tests due to instrumentation bugs.
      debugLog('failed to instrument ' + filename + 'with error: ' + e.stack)
      instrumented = code
    }

    if (_this.fakeRequire) {
      return 'function x () {}'
    } else {
      return instrumented
    }
  }
}

NYC.prototype._handleJs = function (code, filename) {
  var relFile = path.relative(this.cwd, filename)
  // ensure the path has correct casing (see istanbuljs/nyc#269 and nodejs/node#6624)
  filename = path.resolve(this.cwd, relFile)
  return this._maybeInstrumentSource(code, filename, relFile) || code
}

NYC.prototype._addHook = function (type) {
  var handleJs = this._handleJs.bind(this)
  var dummyMatcher = function () { return true } // we do all processing in transformer
  libHook['hook' + type](dummyMatcher, handleJs, { extensions: this.extensions })
}

NYC.prototype._wrapRequire = function () {
  this.extensions.forEach(function (ext) {
    require.extensions[ext] = js
  })
  this._addHook('Require')
}

NYC.prototype._addOtherHooks = function () {
  if (this.hookRunInContext) {
    this._addHook('RunInThisContext')
  }
}

NYC.prototype.cleanup = function () {
  if (!process.env.NYC_CWD) rimraf.sync(this.tempDirectory())
}

NYC.prototype.clearCache = function () {
  if (this.cache) {
    rimraf.sync(this.cacheDirectory)
  }
}

NYC.prototype.createTempDirectory = function () {
  mkdirp.sync(this.tempDirectory())
  if (this.cache) mkdirp.sync(this.cacheDirectory)

  if (this._showProcessTree) {
    mkdirp.sync(this.processInfoDirectory())
  }
}

NYC.prototype.reset = function () {
  this.cleanup()
  this.createTempDirectory()
}

NYC.prototype._wrapExit = function () {
  var _this = this

  // we always want to write coverage
  // regardless of how the process exits.
  onExit(function () {
    _this.writeCoverageFile()
  }, {alwaysLast: true})
}

NYC.prototype.wrap = function (bin) {
  this._wrapRequire()
  this._addOtherHooks()
  this._wrapExit()
  this._loadAdditionalModules()
  return this
}

NYC.prototype.generateUniqueID = function () {
  return md5hex(
    process.hrtime().concat(process.pid).map(String)
  )
}

NYC.prototype.writeCoverageFile = function () {
  var coverage = coverageFinder()
  if (!coverage) return

  // Remove any files that should be excluded but snuck into the coverage
  Object.keys(coverage).forEach(function (absFile) {
    if (!this.exclude.shouldInstrument(absFile)) {
      delete coverage[absFile]
    }
  }, this)

  if (this.cache) {
    Object.keys(coverage).forEach(function (absFile) {
      if (this.hashCache[absFile] && coverage[absFile]) {
        coverage[absFile].contentHash = this.hashCache[absFile]
      }
    }, this)
  } else {
    coverage = this.sourceMaps.remapCoverage(coverage)
  }

  var id = this.generateUniqueID()
  var coverageFilename = path.resolve(this.tempDirectory(), id + '.json')

  fs.writeFileSync(
    coverageFilename,
    JSON.stringify(coverage),
    'utf-8'
  )

  if (!this._showProcessTree) {
    return
  }

  this.processInfo.coverageFilename = coverageFilename

  fs.writeFileSync(
    path.resolve(this.processInfoDirectory(), id + '.json'),
    JSON.stringify(this.processInfo),
    'utf-8'
  )
}

function coverageFinder () {
  var coverage = global.__coverage__
  if (typeof __coverage__ === 'object') coverage = __coverage__
  if (!coverage) coverage = global['__coverage__'] = {}
  return coverage
}

NYC.prototype._getCoverageMapFromAllCoverageFiles = function () {
  var _this = this
  var map = libCoverage.createCoverageMap({})

  this.loadReports().forEach(function (report) {
    map.merge(report)
  })
  map.filter(function (filename) {
    return _this.exclude.shouldInstrument(filename)
  })
  map.data = this.sourceMaps.remapCoverage(map.data)
  return map
}

NYC.prototype.report = function () {
  var tree
  var map = this._getCoverageMapFromAllCoverageFiles()
  var context = libReport.createContext({
    dir: this._reportDir,
    watermarks: this.config.watermarks
  })

  tree = libReport.summarizers.pkg(map)

  this.reporter.forEach(function (_reporter) {
    tree.visit(reports.create(_reporter), context)
  })

  if (this._showProcessTree) {
    this.showProcessTree()
  }
}

NYC.prototype.showProcessTree = function () {
  var processTree = ProcessInfo.buildProcessTree(this._loadProcessInfos())

  console.log(processTree.render(this))
}

NYC.prototype.checkCoverage = function (thresholds, perFile) {
  var map = this._getCoverageMapFromAllCoverageFiles()
  var nyc = this

  if (perFile) {
    map.files().forEach(function (file) {
      // ERROR: Coverage for lines (90.12%) does not meet threshold (120%) for index.js
      nyc._checkCoverage(map.fileCoverageFor(file).toSummary(), thresholds, file)
    })
  } else {
    // ERROR: Coverage for lines (90.12%) does not meet global threshold (120%)
    nyc._checkCoverage(map.getCoverageSummary(), thresholds)
  }

  // process.exitCode was not implemented until v0.11.8.
  if (/^v0\.(1[0-1]\.|[0-9]\.)/.test(process.version) && process.exitCode !== 0) process.exit(process.exitCode)
}

NYC.prototype._checkCoverage = function (summary, thresholds, file) {
  Object.keys(thresholds).forEach(function (key) {
    var coverage = summary[key].pct
    if (coverage < thresholds[key]) {
      process.exitCode = 1
      if (file) {
        console.error('ERROR: Coverage for ' + key + ' (' + coverage + '%) does not meet threshold (' + thresholds[key] + '%) for ' + file)
      } else {
        console.error('ERROR: Coverage for ' + key + ' (' + coverage + '%) does not meet global threshold (' + thresholds[key] + '%)')
      }
    }
  })
}

NYC.prototype._loadProcessInfos = function () {
  var _this = this
  var files = fs.readdirSync(this.processInfoDirectory())

  return files.map(function (f) {
    try {
      return new ProcessInfo(JSON.parse(fs.readFileSync(
        path.resolve(_this.processInfoDirectory(), f),
        'utf-8'
      )))
    } catch (e) { // handle corrupt JSON output.
      return {}
    }
  })
}

NYC.prototype.loadReports = function (filenames) {
  var _this = this
  var files = filenames || fs.readdirSync(this.tempDirectory())

  return files.map(function (f) {
    var report
    try {
      report = JSON.parse(fs.readFileSync(
        path.resolve(_this.tempDirectory(), f),
        'utf-8'
      ))
    } catch (e) { // handle corrupt JSON output.
      return {}
    }

    _this.sourceMaps.reloadCachedSourceMaps(report)
    return report
  })
}

NYC.prototype.tempDirectory = function () {
  return path.resolve(this.cwd, this._tempDirectory)
}

NYC.prototype.processInfoDirectory = function () {
  return path.resolve(this.tempDirectory(), 'processinfo')
}

module.exports = NYC
