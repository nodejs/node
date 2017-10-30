// defaults, types, and shorthands.

var path = require('path')
var url = require('url')
var Stream = require('stream').Stream
var semver = require('semver')
var stableFamily = semver.parse(process.version)
var nopt = require('nopt')
var os = require('os')
var osenv = require('osenv')
var umask = require('../utils/umask')
var hasUnicode = require('has-unicode')

var log
try {
  log = require('npmlog')
} catch (er) {
  var util = require('util')
  log = { warn: function (m) {
    console.warn(m + ' ' + util.format.apply(util, [].slice.call(arguments, 1)))
  } }
}

exports.Umask = Umask
function Umask () {}
function validateUmask (data, k, val) {
  return umask.validate(data, k, val)
}

function validateSemver (data, k, val) {
  if (!semver.valid(val)) return false
  data[k] = semver.valid(val)
}

function validateStream (data, k, val) {
  if (!(val instanceof Stream)) return false
  data[k] = val
}

nopt.typeDefs.semver = { type: semver, validate: validateSemver }
nopt.typeDefs.Stream = { type: Stream, validate: validateStream }
nopt.typeDefs.Umask = { type: Umask, validate: validateUmask }

nopt.invalidHandler = function (k, val, type) {
  log.warn('invalid config', k + '=' + JSON.stringify(val))

  if (Array.isArray(type)) {
    if (type.indexOf(url) !== -1) type = url
    else if (type.indexOf(path) !== -1) type = path
  }

  switch (type) {
    case Umask:
      log.warn('invalid config', 'Must be umask, octal number in range 0000..0777')
      break
    case url:
      log.warn('invalid config', "Must be a full url with 'http://'")
      break
    case path:
      log.warn('invalid config', 'Must be a valid filesystem path')
      break
    case Number:
      log.warn('invalid config', 'Must be a numeric value')
      break
    case Stream:
      log.warn('invalid config', 'Must be an instance of the Stream class')
      break
  }
}

if (!stableFamily || (+stableFamily.minor % 2)) stableFamily = null
else stableFamily = stableFamily.major + '.' + stableFamily.minor

var defaults

var temp = osenv.tmpdir()
var home = osenv.home()

var uidOrPid = process.getuid ? process.getuid() : process.pid

if (home) process.env.HOME = home
else home = path.resolve(temp, 'npm-' + uidOrPid)

var cacheExtra = process.platform === 'win32' ? 'npm-cache' : '.npm'
var cacheRoot = process.platform === 'win32' && process.env.APPDATA || home
var cache = path.resolve(cacheRoot, cacheExtra)

var globalPrefix
Object.defineProperty(exports, 'defaults', {get: function () {
  if (defaults) return defaults

  if (process.env.PREFIX) {
    globalPrefix = process.env.PREFIX
  } else if (process.platform === 'win32') {
    // c:\node\node.exe --> prefix=c:\node\
    globalPrefix = path.dirname(process.execPath)
  } else {
    // /usr/local/bin/node --> prefix=/usr/local
    globalPrefix = path.dirname(path.dirname(process.execPath))

    // destdir only is respected on Unix
    if (process.env.DESTDIR) {
      globalPrefix = path.join(process.env.DESTDIR, globalPrefix)
    }
  }

  defaults = {
    access: null,
    'allow-same-version': false,
    'always-auth': false,
    also: null,
    'auth-type': 'legacy',

    'bin-links': true,
    browser: null,

    ca: null,
    cafile: null,

    cache: cache,

    'cache-lock-stale': 60000,
    'cache-lock-retries': 10,
    'cache-lock-wait': 10000,

    'cache-max': Infinity,
    'cache-min': 10,

    cert: null,

    cidr: null,

    color: true,
    depth: Infinity,
    description: true,
    dev: false,
    'dry-run': false,
    editor: osenv.editor(),
    'engine-strict': false,
    force: false,

    'fetch-retries': 2,
    'fetch-retry-factor': 10,
    'fetch-retry-mintimeout': 10000,
    'fetch-retry-maxtimeout': 60000,

    git: 'git',
    'git-tag-version': true,
    'commit-hooks': true,

    global: false,
    globalconfig: path.resolve(globalPrefix, 'etc', 'npmrc'),
    'global-style': false,
    group: process.platform === 'win32' ? 0
            : process.env.SUDO_GID || (process.getgid && process.getgid()),
    'ham-it-up': false,
    heading: 'npm',
    'if-present': false,
    'ignore-prepublish': false,
    'ignore-scripts': false,
    'init-module': path.resolve(home, '.npm-init.js'),
    'init-author-name': '',
    'init-author-email': '',
    'init-author-url': '',
    'init-version': '1.0.0',
    'init-license': 'ISC',
    json: false,
    key: null,
    'legacy-bundling': false,
    link: false,
    'local-address': undefined,
    loglevel: 'notice',
    logstream: process.stderr,
    'logs-max': 10,
    long: false,
    maxsockets: 50,
    message: '%s',
    'metrics-registry': null,
    'node-version': process.version,
    'offline': false,
    'onload-script': false,
    only: null,
    optional: true,
    otp: null,
    'package-lock': true,
    parseable: false,
    'prefer-offline': false,
    'prefer-online': false,
    prefix: globalPrefix,
    production: process.env.NODE_ENV === 'production',
    'progress': !process.env.TRAVIS && !process.env.CI,
    proxy: null,
    'https-proxy': null,
    'user-agent': 'npm/{npm-version} ' +
                    'node/{node-version} ' +
                    '{platform} ' +
                    '{arch}',
    'read-only': false,
    'rebuild-bundle': true,
    registry: 'https://registry.npmjs.org/',
    rollback: true,
    save: true,
    'save-bundle': false,
    'save-dev': false,
    'save-exact': false,
    'save-optional': false,
    'save-prefix': '^',
    'save-prod': false,
    scope: '',
    'script-shell': null,
    'scripts-prepend-node-path': 'warn-only',
    searchopts: '',
    searchexclude: null,
    searchlimit: 20,
    searchstaleness: 15 * 60,
    'send-metrics': false,
    shell: osenv.shell(),
    shrinkwrap: true,
    'sign-git-tag': false,
    'sso-poll-frequency': 500,
    'sso-type': 'oauth',
    'strict-ssl': true,
    tag: 'latest',
    'tag-version-prefix': 'v',
    timing: false,
    tmp: temp,
    unicode: hasUnicode(),
    'unsafe-perm': process.platform === 'win32' ||
                     process.platform === 'cygwin' ||
                     !(process.getuid && process.setuid &&
                       process.getgid && process.setgid) ||
                     process.getuid() !== 0,
    usage: false,
    user: process.platform === 'win32' ? 0 : 'nobody',
    userconfig: path.resolve(home, '.npmrc'),
    umask: process.umask ? process.umask() : umask.fromString('022'),
    version: false,
    versions: false,
    viewer: process.platform === 'win32' ? 'browser' : 'man',

    _exit: true
  }

  return defaults
}})

exports.types = {
  access: [null, 'restricted', 'public'],
  'allow-same-version': Boolean,
  'always-auth': Boolean,
  also: [null, 'dev', 'development'],
  'auth-type': ['legacy', 'sso', 'saml', 'oauth'],
  'bin-links': Boolean,
  browser: [null, String],
  ca: [null, String, Array],
  cafile: path,
  cache: path,
  'cache-lock-stale': Number,
  'cache-lock-retries': Number,
  'cache-lock-wait': Number,
  'cache-max': Number,
  'cache-min': Number,
  cert: [null, String],
  cidr: [null, String, Array],
  color: ['always', Boolean],
  depth: Number,
  description: Boolean,
  dev: Boolean,
  'dry-run': Boolean,
  editor: String,
  'engine-strict': Boolean,
  force: Boolean,
  'fetch-retries': Number,
  'fetch-retry-factor': Number,
  'fetch-retry-mintimeout': Number,
  'fetch-retry-maxtimeout': Number,
  git: String,
  'git-tag-version': Boolean,
  'commit-hooks': Boolean,
  global: Boolean,
  globalconfig: path,
  'global-style': Boolean,
  group: [Number, String],
  'https-proxy': [null, url],
  'user-agent': String,
  'ham-it-up': Boolean,
  'heading': String,
  'if-present': Boolean,
  'ignore-prepublish': Boolean,
  'ignore-scripts': Boolean,
  'init-module': path,
  'init-author-name': String,
  'init-author-email': String,
  'init-author-url': ['', url],
  'init-license': String,
  'init-version': semver,
  json: Boolean,
  key: [null, String],
  'legacy-bundling': Boolean,
  link: Boolean,
  // local-address must be listed as an IP for a local network interface
  // must be IPv4 due to node bug
  'local-address': getLocalAddresses(),
  loglevel: ['silent', 'error', 'warn', 'notice', 'http', 'timing', 'info', 'verbose', 'silly'],
  logstream: Stream,
  'logs-max': Number,
  long: Boolean,
  maxsockets: Number,
  message: String,
  'metrics-registry': [null, String],
  'node-version': [null, semver],
  offline: Boolean,
  'onload-script': [null, String],
  only: [null, 'dev', 'development', 'prod', 'production'],
  optional: Boolean,
  'package-lock': Boolean,
  otp: Number,
  parseable: Boolean,
  'prefer-offline': Boolean,
  'prefer-online': Boolean,
  prefix: path,
  production: Boolean,
  progress: Boolean,
  proxy: [null, false, url], // allow proxy to be disabled explicitly
  'read-only': Boolean,
  'rebuild-bundle': Boolean,
  registry: [null, url],
  rollback: Boolean,
  save: Boolean,
  'save-bundle': Boolean,
  'save-dev': Boolean,
  'save-exact': Boolean,
  'save-optional': Boolean,
  'save-prefix': String,
  'save-prod': Boolean,
  scope: String,
  'script-shell': [null, String],
  'scripts-prepend-node-path': [false, true, 'auto', 'warn-only'],
  searchopts: String,
  searchexclude: [null, String],
  searchlimit: Number,
  searchstaleness: Number,
  'send-metrics': Boolean,
  shell: String,
  shrinkwrap: Boolean,
  'sign-git-tag': Boolean,
  'sso-poll-frequency': Number,
  'sso-type': [null, 'oauth', 'saml'],
  'strict-ssl': Boolean,
  tag: String,
  timing: Boolean,
  tmp: path,
  unicode: Boolean,
  'unsafe-perm': Boolean,
  usage: Boolean,
  user: [Number, String],
  userconfig: path,
  umask: Umask,
  version: Boolean,
  'tag-version-prefix': String,
  versions: Boolean,
  viewer: String,
  _exit: Boolean
}

function getLocalAddresses () {
  var interfaces
  // #8094: some environments require elevated permissions to enumerate
  // interfaces, and synchronously throw EPERM when run without
  // elevated privileges
  try {
    interfaces = os.networkInterfaces()
  } catch (e) {
    interfaces = {}
  }

  return Object.keys(interfaces).map(function (nic) {
    return interfaces[nic].filter(function (addr) {
      return addr.family === 'IPv4'
    })
    .map(function (addr) {
      return addr.address
    })
  }).reduce(function (curr, next) {
    return curr.concat(next)
  }, []).concat(undefined)
}

exports.shorthands = {
  s: ['--loglevel', 'silent'],
  d: ['--loglevel', 'info'],
  dd: ['--loglevel', 'verbose'],
  ddd: ['--loglevel', 'silly'],
  noreg: ['--no-registry'],
  N: ['--no-registry'],
  reg: ['--registry'],
  'no-reg': ['--no-registry'],
  silent: ['--loglevel', 'silent'],
  verbose: ['--loglevel', 'verbose'],
  quiet: ['--loglevel', 'warn'],
  q: ['--loglevel', 'warn'],
  h: ['--usage'],
  H: ['--usage'],
  '?': ['--usage'],
  help: ['--usage'],
  v: ['--version'],
  f: ['--force'],
  desc: ['--description'],
  'no-desc': ['--no-description'],
  'local': ['--no-global'],
  l: ['--long'],
  m: ['--message'],
  p: ['--parseable'],
  porcelain: ['--parseable'],
  readonly: ['--read-only'],
  g: ['--global'],
  S: ['--save'],
  D: ['--save-dev'],
  E: ['--save-exact'],
  O: ['--save-optional'],
  P: ['--save-prod'],
  y: ['--yes'],
  n: ['--no-yes'],
  B: ['--save-bundle'],
  C: ['--prefix']
}
