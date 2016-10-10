module.exports.cli = require('./bin/cmd')

module.exports.linter = Linter

var defaults = require('defaults')
var deglob = require('deglob')
var extend = require('xtend')
var findRoot = require('find-root')
var pkgConfig = require('pkg-config')

var DEFAULT_PATTERNS = [
  '**/*.js',
  '**/*.jsx'
]

var DEFAULT_IGNORE = [
  '**/*.min.js',
  '**/bundle.js',
  'coverage/**',
  'node_modules/**',
  'vendor/**'
]

function Linter (opts) {
  var self = this
  if (!(self instanceof Linter)) return new Linter(opts)
  if (!opts) opts = {}

  self.cmd = opts.cmd || 'standard'
  self.eslint = opts.eslint
  self.cwd = opts.cwd
  if (!self.eslint) throw new Error('opts.eslint option is required')

  self.eslintConfig = defaults(opts.eslintConfig, {
    useEslintrc: false,
    globals: [],
    plugins: [],
    envs: [],
    rules: {}
  })
  if (!self.eslintConfig) {
    throw new Error('No eslintConfig passed.')
  }
}

/**
 * Lint text to enforce JavaScript Style.
 *
 * @param {string} text                   file text to lint
 * @param {Object=} opts                  options object
 * @param {Array.<string>=} opts.globals  custom global variables to declare
 * @param {Array.<string>=} opts.plugins  custom eslint plugins
 * @param {Array.<string>=} opts.envs     custom eslint environment
 * @param {Object=} opts.rules            custom eslint rules
 * @param {string=} opts.parser           custom js parser (e.g. babel-eslint)
 * @param {function(Error, Object)} cb    callback
 */
Linter.prototype.lintText = function (text, opts, cb) {
  var self = this
  if (typeof opts === 'function') return self.lintText(text, null, opts)
  opts = self.parseOpts(opts)

  var result
  try {
    result = new self.eslint.CLIEngine(opts.eslintConfig).executeOnText(text)
  } catch (err) {
    return nextTick(cb, err)
  }
  return nextTick(cb, null, result)
}

/**
 * Lint files to enforce JavaScript Style.
 *
 * @param {Array.<string>} files          file globs to lint
 * @param {Object=} opts                  options object
 * @param {Array.<string>=} opts.ignore   file globs to ignore (has sane defaults)
 * @param {string=} opts.cwd              current working directory (default: process.cwd())
 * @param {Array.<string>=} opts.globals  custom global variables to declare
 * @param {Array.<string>=} opts.plugins  custom eslint plugins
 * @param {Array.<string>=} opts.envs     custom eslint environment
 * @param {Object=} opts.rules            custom eslint rules
 * @param {string=} opts.parser           custom js parser (e.g. babel-eslint)
 * @param {function(Error, Object)} cb    callback
 */
Linter.prototype.lintFiles = function (files, opts, cb) {
  var self = this
  if (typeof opts === 'function') return self.lintFiles(files, null, opts)
  opts = self.parseOpts(opts)

  if (typeof files === 'string') files = [ files ]
  if (files.length === 0) files = DEFAULT_PATTERNS

  var deglobOpts = {
    ignore: opts.ignore,
    cwd: opts.cwd,
    useGitIgnore: true,
    usePackageJson: true,
    configKey: self.cmd
  }

  deglob(files, deglobOpts, function (err, allFiles) {
    if (err) return cb(err)
    // undocumented – do not use (used by bin/cmd.js)
    if (opts._onFiles) opts._onFiles(allFiles)

    var result
    try {
      result = new self.eslint.CLIEngine(opts.eslintConfig).executeOnFiles(allFiles)
    } catch (err) {
      return cb(err)
    }
    return cb(null, result)
  })
}

Linter.prototype.parseOpts = function (opts) {
  var self = this

  if (!opts) opts = {}
  opts = extend(opts)
  opts.eslintConfig = extend(self.eslintConfig)

  if (!opts.cwd) opts.cwd = self.cwd || process.cwd()

  if (!opts.ignore) opts.ignore = []
  opts.ignore = opts.ignore.concat(DEFAULT_IGNORE)

  setGlobals(opts.globals || opts.global)
  setPlugins(opts.plugins || opts.plugin)
  setEnvs(opts.envs || opts.env)
  setRules(opts.rules || opts.rule)
  setParser(opts.parser)

  var root
  try { root = findRoot(opts.cwd) } catch (e) {}
  if (root) {
    var packageOpts = pkgConfig(self.cmd, { root: false, cwd: opts.cwd })

    if (packageOpts) {
      setGlobals(packageOpts.globals || packageOpts.global)
      setPlugins(packageOpts.plugins || packageOpts.plugin)
      setRules(packageOpts.rules || packageOpts.rule)
      setEnvs(packageOpts.envs || packageOpts.env)
      if (!opts.parser) setParser(packageOpts.parser)
    }
  }

  function setGlobals (globals) {
    if (!globals) return
    opts.eslintConfig.globals = self.eslintConfig.globals.concat(globals)
  }

  function setPlugins (plugins) {
    if (!plugins) return
    opts.eslintConfig.plugins = self.eslintConfig.plugins.concat(plugins)
  }

  function setRules (rules) {
    if (!rules) return
    opts.eslintConfig.rules = extend(opts.eslintConfig.rules, rules)
  }

  function setEnvs (envs) {
    if (!envs) return
    if (!Array.isArray(envs) && typeof envs !== 'string') {
      // envs can be an object in `package.json`
      envs = Object.keys(envs).filter(function (env) { return envs[env] })
    }
    opts.eslintConfig.envs = self.eslintConfig.envs.concat(envs)
  }

  function setParser (parser) {
    if (!parser) return
    opts.eslintConfig.parser = parser
  }

  return opts
}

function nextTick (cb, err, val) {
  process.nextTick(function () {
    cb(err, val)
  })
}
