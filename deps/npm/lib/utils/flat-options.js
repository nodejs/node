// return a flattened config object with canonical names suitable for
// passing to dependencies like arborist, pacote, npm-registry-fetch, etc.

const log = require('npmlog')
const crypto = require('crypto')
const npmSession = crypto.randomBytes(8).toString('hex')
log.verbose('npm-session', npmSession)
const { join } = require('path')

const buildOmitList = npm => {
  const include = npm.config.get('include') || []
  const omit = new Set((npm.config.get('omit') || [])
    .filter(type => !include.includes(type)))
  const only = npm.config.get('only')

  if (/^prod(uction)?$/.test(only) || npm.config.get('production')) {
    omit.add('dev')
  }

  if (/dev/.test(npm.config.get('also'))) {
    omit.delete('dev')
  }

  if (npm.config.get('dev')) {
    omit.delete('dev')
  }

  if (npm.config.get('optional') === false) {
    omit.add('optional')
  }

  npm.config.set('omit', [...omit])
  return [...omit]
}

const flatOptions = npm => npm.flatOptions || Object.freeze({
  // Note that many of these do not come from configs or cli flags
  // per se, though they may be implied or defined by them.
  log,
  npmSession,
  dmode: npm.modes.exec,
  fmode: npm.modes.file,
  umask: npm.modes.umask,
  hashAlgorithm: 'sha1', // XXX should this be sha512?
  color: !!npm.color,
  includeStaged: npm.config.get('include-staged'),

  preferDedupe: npm.config.get('prefer-dedupe'),
  ignoreScripts: npm.config.get('ignore-scripts'),

  projectScope: npm.projectScope,
  npmVersion: npm.version,
  nodeVersion: npm.config.get('node-version'),
  // npm.command is not set until AFTER flatOptions are defined
  // so we need to make this a getter.
  get npmCommand () {
    return npm.command
  },

  tmp: npm.tmp,
  cache: join(npm.config.get('cache'), '_cacache'),
  prefix: npm.prefix,
  globalPrefix: npm.globalPrefix,
  localPrefix: npm.localPrefix,
  global: npm.config.get('global'),

  metricsRegistry: npm.config.get('metrics-registry') ||
    npm.config.get('registry'),
  sendMetrics: npm.config.get('send-metrics'),
  registry: npm.config.get('registry'),
  scope: npm.config.get('scope'),
  access: npm.config.get('access'),
  alwaysAuth: npm.config.get('always-auth'),
  audit: npm.config.get('audit'),
  auditLevel: npm.config.get('audit-level'),
  authType: npm.config.get('auth-type'),
  ssoType: npm.config.get('sso-type'),
  ssoPollFrequency: npm.config.get('sso-poll-frequency'),
  before: npm.config.get('before'),
  browser: npm.config.get('browser'),
  ca: npm.config.get('ca'),
  cafile: npm.config.get('cafile'),
  cert: npm.config.get('cert'),
  key: npm.config.get('key'),

  // XXX remove these when we don't use lockfile any more, once
  // arborist is handling the installation process
  cacheLockRetries: npm.config.get('cache-lock-retries'),
  cacheLockStale: npm.config.get('cache-lock-stale'),
  cacheLockWait: npm.config.get('cache-lock-wait'),
  lockFile: {
    retries: npm.config.get('cache-lock-retries'),
    stale: npm.config.get('cache-lock-stale'),
    wait: npm.config.get('cache-lock-wait')
  },

  // XXX remove these once no longer used
  get cacheMax () {
    return npm.config.get('cache-max')
  },
  get cacheMin () {
    return npm.config.get('cache-min')
  },

  // token creation options
  cidr: npm.config.get('cidr'),
  readOnly: npm.config.get('read-only'),

  // npm version options
  preid: npm.config.get('preid'),
  tagVersionPrefix: npm.config.get('tag-version-prefix'),
  allowSameVersion: npm.config.get('allow-same-version'),

  // npm version git options
  message: npm.config.get('message'),
  commitHooks: npm.config.get('commit-hooks'),
  gitTagVersion: npm.config.get('git-tag-version'),
  signGitCommit: npm.config.get('sign-git-commit'),
  signGitTag: npm.config.get('sign-git-tag'),

  // only used for npm ls in v7, not update
  depth: npm.config.get('depth'),
  all: npm.config.get('all'),

  // Output configs
  unicode: npm.config.get('unicode'),
  json: npm.config.get('json'),
  long: npm.config.get('long'),
  parseable: npm.config.get('parseable'),

  // options for npm search
  search: {
    description: npm.config.get('description'),
    exclude: npm.config.get('searchexclude'),
    limit: npm.config.get('searchlimit') || 20,
    opts: npm.config.get('searchopts'),
    staleness: npm.config.get('searchstaleness')
  },

  dryRun: npm.config.get('dry-run'),
  engineStrict: npm.config.get('engine-strict'),

  retry: {
    retries: npm.config.get('fetch-retries'),
    factor: npm.config.get('fetch-retry-factor'),
    maxTimeout: npm.config.get('fetch-retry-maxtimeout'),
    minTimeout: npm.config.get('fetch-retry-mintimeout')
  },

  timeout: npm.config.get('fetch-timeout'),

  force: npm.config.get('force'),

  formatPackageLock: npm.config.get('format-package-lock'),
  fund: npm.config.get('fund'),

  // binary locators
  git: npm.config.get('git'),
  npmBin: require.main.filename,
  nodeBin: process.env.NODE || process.execPath,
  viewer: npm.config.get('viewer'),
  editor: npm.config.get('editor'),

  // configs that affect how we build trees
  binLinks: npm.config.get('bin-links'),
  rebuildBundle: npm.config.get('rebuild-bundle'),
  packageLock: npm.config.get('package-lock'),
  packageLockOnly: npm.config.get('package-lock-only'),
  globalStyle: npm.config.get('global-style'),
  legacyBundling: npm.config.get('legacy-bundling'),
  scriptShell: npm.config.get('script-shell') || undefined,
  shell: npm.config.get('shell'),
  omit: buildOmitList(npm),
  legacyPeerDeps: npm.config.get('legacy-peer-deps'),
  strictPeerDeps: npm.config.get('strict-peer-deps'),

  // npx stuff
  call: npm.config.get('call'),
  package: npm.config.get('package'),

  // used to build up the appropriate {add:{...}} options to Arborist.reify
  save: npm.config.get('save'),
  saveBundle: npm.config.get('save-bundle') && !npm.config.get('save-peer'),
  saveType: npm.config.get('save-optional') && npm.config.get('save-peer')
    ? 'peerOptional'
    : npm.config.get('save-optional') ? 'optional'
    : npm.config.get('save-dev') ? 'dev'
    : npm.config.get('save-peer') ? 'peer'
    : npm.config.get('save-prod') ? 'prod'
    : null,
  savePrefix: npm.config.get('save-exact') ? ''
  : npm.config.get('save-prefix'),

  // configs for npm-registry-fetch
  otp: npm.config.get('otp'),
  offline: npm.config.get('offline'),
  preferOffline: getPreferOffline(npm),
  preferOnline: getPreferOnline(npm),
  strictSSL: npm.config.get('strict-ssl'),
  defaultTag: npm.config.get('tag'),
  get tag () {
    return npm.config.get('tag')
  },
  userAgent: npm.config.get('user-agent'),

  // yes, it's fine, just do it, jeez, stop asking
  yes: npm.config.get('yes'),

  ...getScopesAndAuths(npm),

  // npm fund exclusive option to select an item from a funding list
  which: npm.config.get('which'),

  // socks proxy can be configured in https-proxy or proxy field
  // note that the various (HTTPS_|HTTP_|)PROXY environs will be
  // respected if this is not set.
  proxy: npm.config.get('https-proxy') || npm.config.get('proxy'),
  noProxy: npm.config.get('noproxy')
})

const getPreferOnline = npm => {
  const po = npm.config.get('prefer-online')
  if (po !== undefined) {
    return po
  }
  return npm.config.get('cache-max') <= 0
}

const getPreferOffline = npm => {
  const po = npm.config.get('prefer-offline')
  if (po !== undefined) {
    return po
  }
  return npm.config.get('cache-min') >= 9999
}

// pull out all the @scope:<key> and //host:key config fields
// these are used by npm-registry-fetch for authing against registries
const getScopesAndAuths = npm => {
  const scopesAndAuths = {}
  // pull out all the @scope:... configs into a flat object.
  for (const key in npm.config.list[0]) {
    if (/@.*:registry$/i.test(key) || /^\/\//.test(key)) {
      scopesAndAuths[key] = npm.config.get(key)
    }
  }
  return scopesAndAuths
}

module.exports = flatOptions
