// return a flattened config object with canonical names suitable for
// passing to dependencies like arborist, pacote, npm-registry-fetch, etc.

const log = require('npmlog')
const crypto = require('crypto')
const querystring = require('querystring')
const npmSession = crypto.randomBytes(8).toString('hex')
log.verbose('npm-session', npmSession)
const { join } = require('path')

const buildOmitList = obj => {
  const include = obj.include || []
  const omit = new Set((obj.omit || [])
    .filter(type => !include.includes(type)))
  const only = obj.only

  if (/^prod(uction)?$/.test(only) || obj.production)
    omit.add('dev')

  if (/dev/.test(obj.also))
    omit.delete('dev')

  if (obj.dev)
    omit.delete('dev')

  if (obj.optional === false)
    omit.add('optional')

  obj.omit = [...omit]

  // it would perhaps make more sense to put this in @npmcli/config, but
  // since we can set dev to be omitted in multiple various legacy ways,
  // it's better to set it here once it's all resolved.
  if (obj.omit.includes('dev'))
    process.env.NODE_ENV = 'production'

  return [...omit]
}

// turn an object with npm-config style keys into an options object
// with camelCase values.  This doesn't account for the stuff that is
// not pulled from the config keys, that's all handled only for the
// main function which acts on the npm object itself.  Used by the
// flatOptions generator, and by the publishConfig handling logic.
const flatten = obj => ({
  includeStaged: obj['include-staged'],
  preferDedupe: obj['prefer-dedupe'],
  ignoreScripts: obj['ignore-scripts'],
  nodeVersion: obj['node-version'],
  cache: join(obj.cache, '_cacache'),
  global: obj.global,

  registry: obj.registry,
  scope: obj.scope,
  access: obj.access,
  alwaysAuth: obj['always-auth'],
  audit: obj.audit,
  auditLevel: obj['audit-level'],
  _auth: obj._auth,
  authType: obj['auth-type'],
  ssoType: obj['sso-type'],
  ssoPollFrequency: obj['sso-poll-frequency'],
  before: obj.before,
  browser: obj.browser,
  ca: obj.ca,
  cafile: obj.cafile,
  cert: obj.cert,
  key: obj.key,

  // token creation options
  cidr: obj.cidr,
  readOnly: obj['read-only'],

  // npm version options
  preid: obj.preid,
  tagVersionPrefix: obj['tag-version-prefix'],
  allowSameVersion: obj['allow-same-version'],

  // npm version git options
  message: obj.message,
  commitHooks: obj['commit-hooks'],
  gitTagVersion: obj['git-tag-version'],
  signGitCommit: obj['sign-git-commit'],
  signGitTag: obj['sign-git-tag'],

  // only used for npm ls in v7, not update
  depth: obj.depth,
  all: obj.all,

  // Output configs
  unicode: obj.unicode,
  json: obj.json,
  long: obj.long,
  parseable: obj.parseable,

  // options for npm search
  search: {
    description: obj.description,
    exclude: obj.searchexclude,
    limit: obj.searchlimit || 20,
    opts: querystring.parse(obj.searchopts),
    staleness: obj.searchstaleness,
  },

  dryRun: obj['dry-run'],
  engineStrict: obj['engine-strict'],

  retry: {
    retries: obj['fetch-retries'],
    factor: obj['fetch-retry-factor'],
    maxTimeout: obj['fetch-retry-maxtimeout'],
    minTimeout: obj['fetch-retry-mintimeout'],
  },

  timeout: obj['fetch-timeout'],

  force: obj.force,

  formatPackageLock: obj['format-package-lock'],
  fund: obj.fund,

  // binary locators
  git: obj.git,
  viewer: obj.viewer,
  editor: obj.editor,

  // configs that affect how we build trees
  binLinks: obj['bin-links'],
  rebuildBundle: obj['rebuild-bundle'],
  // --no-shrinkwrap is the same as --no-package-lock
  packageLock: !(obj['package-lock'] === false ||
    obj.shrinkwrap === false),
  packageLockOnly: obj['package-lock-only'],
  globalStyle: obj['global-style'],
  legacyBundling: obj['legacy-bundling'],
  scriptShell: obj['script-shell'] || undefined,
  shell: obj.shell,
  omit: buildOmitList(obj),
  legacyPeerDeps: obj['legacy-peer-deps'],
  strictPeerDeps: obj['strict-peer-deps'],

  // npx stuff
  call: obj.call,
  package: obj.package,

  // used to build up the appropriate {add:{...}} options to Arborist.reify
  save: obj.save,
  saveBundle: obj['save-bundle'] && !obj['save-peer'],
  saveType: obj['save-optional'] && obj['save-peer']
    ? 'peerOptional'
    : obj['save-optional'] ? 'optional'
    : obj['save-dev'] ? 'dev'
    : obj['save-peer'] ? 'peer'
    : obj['save-prod'] ? 'prod'
    : null,
  savePrefix: obj['save-exact'] ? ''
  : obj['save-prefix'],

  // configs for npm-registry-fetch
  otp: obj.otp,
  offline: obj.offline,
  preferOffline: getPreferOffline(obj),
  preferOnline: getPreferOnline(obj),
  strictSSL: obj['strict-ssl'],
  defaultTag: obj.tag,
  userAgent: obj['user-agent'],

  // yes, it's fine, just do it, jeez, stop asking
  yes: obj.yes,

  ...getScopesAndAuths(obj),

  // npm fund exclusive option to select an item from a funding list
  which: obj.which,

  // socks proxy can be configured in https-proxy or proxy field
  // note that the various (HTTPS_|HTTP_|]PROXY environs will be
  // respected if this is not set.
  proxy: obj['https-proxy'] || obj.proxy,
  noProxy: obj.noproxy,
})

const flatOptions = npm => npm.flatOptions || Object.freeze({
  // flatten the config object
  ...flatten(npm.config.list[0]),

  // Note that many of these do not come from configs or cli flags
  // per se, though they may be implied or defined by them.
  log,
  npmSession,
  dmode: npm.modes.exec,
  fmode: npm.modes.file,
  umask: npm.modes.umask,
  hashAlgorithm: 'sha1', // XXX should this be sha512?
  color: !!npm.color,
  projectScope: npm.projectScope,
  npmVersion: npm.version,

  // npm.command is not set until AFTER flatOptions are defined
  // so we need to make this a getter.
  get npmCommand () {
    return npm.command
  },

  tmp: npm.tmp,
  prefix: npm.prefix,
  globalPrefix: npm.globalPrefix,
  localPrefix: npm.localPrefix,
  npmBin: require.main && require.main.filename,
  nodeBin: process.env.NODE || process.execPath,
  get tag () {
    return npm.config.get('tag')
  },
})

const getPreferOnline = obj => {
  const po = obj['prefer-online']
  if (po !== undefined)
    return po

  return obj['cache-max'] <= 0
}

const getPreferOffline = obj => {
  const po = obj['prefer-offline']
  if (po !== undefined)
    return po

  return obj['cache-min'] >= 9999
}

// pull out all the @scope:<key> and //host:key config fields
// these are used by npm-registry-fetch for authing against registries
const getScopesAndAuths = obj => {
  const scopesAndAuths = {}
  // pull out all the @scope:... configs into a flat object.
  for (const key in obj) {
    if (/@.*:registry$/i.test(key) || /^\/\//.test(key))
      scopesAndAuths[key] = obj[key]
  }
  return scopesAndAuths
}

module.exports = Object.assign(flatOptions, { flatten })
