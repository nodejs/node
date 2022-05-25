const definitions = {}
module.exports = definitions

const Definition = require('./definition.js')

const { version: npmVersion } = require('../../../package.json')
const ciDetect = require('@npmcli/ci-detect')
const ciName = ciDetect()
const querystring = require('querystring')
const { isWindows } = require('../is-windows.js')
const { join } = require('path')

// used by cafile flattening to flatOptions.ca
const fs = require('fs')
const maybeReadFile = file => {
  try {
    return fs.readFileSync(file, 'utf8')
  } catch (er) {
    if (er.code !== 'ENOENT') {
      throw er
    }
    return null
  }
}

const buildOmitList = obj => {
  const include = obj.include || []
  const omit = obj.omit || []

  const only = obj.only
  if (/^prod(uction)?$/.test(only) || obj.production) {
    omit.push('dev')
  } else if (obj.production === false) {
    include.push('dev')
  }

  if (/^dev/.test(obj.also)) {
    include.push('dev')
  }

  if (obj.dev) {
    include.push('dev')
  }

  if (obj.optional === false) {
    omit.push('optional')
  } else if (obj.optional === true) {
    include.push('optional')
  }

  obj.omit = [...new Set(omit)].filter(type => !include.includes(type))
  obj.include = [...new Set(include)]

  if (obj.omit.includes('dev')) {
    process.env.NODE_ENV = 'production'
  }

  return obj.omit
}

const editor = process.env.EDITOR ||
  process.env.VISUAL ||
  (isWindows ? 'notepad.exe' : 'vi')

const shell = isWindows ? process.env.ComSpec || 'cmd'
  : process.env.SHELL || 'sh'

const { tmpdir, networkInterfaces } = require('os')
const getLocalAddresses = () => {
  try {
    return Object.values(networkInterfaces()).map(
      int => int.map(({ address }) => address)
    ).reduce((set, addrs) => set.concat(addrs), [null])
  } catch (e) {
    return [null]
  }
}

const unicode = /UTF-?8$/i.test(
  process.env.LC_ALL ||
  process.env.LC_CTYPE ||
  process.env.LANG
)

// use LOCALAPPDATA on Windows, if set
// https://github.com/npm/cli/pull/899
const cacheRoot = (isWindows && process.env.LOCALAPPDATA) || '~'
const cacheExtra = isWindows ? 'npm-cache' : '.npm'
const cache = `${cacheRoot}/${cacheExtra}`

const Config = require('@npmcli/config')

// TODO: refactor these type definitions so that they are less
// weird to pull out of the config module.
// TODO: use better type definition/validation API, nopt's is so weird.
const {
  typeDefs: {
    semver: { type: semver },
    Umask: { type: Umask },
    url: { type: url },
    path: { type: path },
  },
} = Config

const define = (key, def) => {
  /* istanbul ignore if - this should never happen, prevents mistakes below */
  if (definitions[key]) {
    throw new Error(`defining key more than once: ${key}`)
  }
  definitions[key] = new Definition(key, def)
}

// basic flattening function, just copy it over camelCase
const flatten = (key, obj, flatOptions) => {
  const camel = key.replace(/-([a-z])/g, (_0, _1) => _1.toUpperCase())
  flatOptions[camel] = obj[key]
}

// TODO:
// Instead of having each definition provide a flatten method,
// provide the (?list of?) flat option field(s?) that it impacts.
// When that config is set, we mark the relevant flatOption fields
// dirty.  Then, a getter for that field defines how we actually
// set it.
//
// So, `save-dev`, `save-optional`, `save-prod`, et al would indicate
// that they affect the `saveType` flat option.  Then the config.flat
// object has a `get saveType () { ... }` that looks at the "real"
// config settings from files etc and returns the appropriate value.
//
// Getters will also (maybe?) give us a hook to audit flat option
// usage, so we can document and group these more appropriately.
//
// This will be a problem with cases where we currently do:
// const opts = { ...npm.flatOptions, foo: 'bar' }, but we can maybe
// instead do `npm.config.set('foo', 'bar')` prior to passing the
// config object down where it needs to go.
//
// This way, when we go hunting for "where does saveType come from anyway!?"
// while fixing some Arborist bug, we won't have to hunt through too
// many places.

// Define all config keys we know about

define('_auth', {
  default: null,
  type: [null, String],
  description: `
    A basic-auth string to use when authenticating against the npm registry.
    This will ONLY be used to authenticate against the npm registry.  For other
    registries you will need to scope it like "//other-registry.tld/:_auth"

    Warning: This should generally not be set via a command-line option.  It
    is safer to use a registry-provided authentication bearer token stored in
    the ~/.npmrc file by running \`npm login\`.
  `,
  flatten,
})

define('access', {
  default: null,
  defaultDescription: `
    'restricted' for scoped packages, 'public' for unscoped packages
  `,
  type: [null, 'restricted', 'public'],
  description: `
    When publishing scoped packages, the access level defaults to
    \`restricted\`.  If you want your scoped package to be publicly viewable
    (and installable) set \`--access=public\`. The only valid values for
    \`access\` are \`public\` and \`restricted\`. Unscoped packages _always_
    have an access level of \`public\`.

    Note: Using the \`--access\` flag on the \`npm publish\` command will only
    set the package access level on the initial publish of the package. Any
    subsequent \`npm publish\` commands using the \`--access\` flag will not
    have an effect to the access level.  To make changes to the access level
    after the initial publish use \`npm access\`.
  `,
  flatten,
})

define('all', {
  default: false,
  type: Boolean,
  short: 'a',
  description: `
    When running \`npm outdated\` and \`npm ls\`, setting \`--all\` will show
    all outdated or installed packages, rather than only those directly
    depended upon by the current project.
  `,
  flatten,
})

define('allow-same-version', {
  default: false,
  type: Boolean,
  description: `
    Prevents throwing an error when \`npm version\` is used to set the new
    version to the same value as the current version.
  `,
  flatten,
})

define('also', {
  default: null,
  type: [null, 'dev', 'development'],
  description: `
    When set to \`dev\` or \`development\`, this is an alias for
    \`--include=dev\`.
  `,
  deprecated: 'Please use --include=dev instead.',
  flatten (key, obj, flatOptions) {
    definitions.omit.flatten('omit', obj, flatOptions)
  },
})

define('audit', {
  default: true,
  type: Boolean,
  description: `
    When "true" submit audit reports alongside the current npm command to the
    default registry and all registries configured for scopes.  See the
    documentation for [\`npm audit\`](/commands/npm-audit) for details on what
    is submitted.
  `,
  flatten,
})

define('audit-level', {
  default: null,
  type: [null, 'info', 'low', 'moderate', 'high', 'critical', 'none'],
  description: `
    The minimum level of vulnerability for \`npm audit\` to exit with
    a non-zero exit code.
  `,
  flatten,
})

define('auth-type', {
  default: 'legacy',
  type: ['legacy', 'sso', 'saml', 'oauth'],
  deprecated: `
    This method of SSO/SAML/OAuth is deprecated and will be removed in
    a future version of npm in favor of web-based login.
  `,
  description: `
    What authentication strategy to use with \`adduser\`/\`login\`.
  `,
  flatten,
})

define('before', {
  default: null,
  type: [null, Date],
  description: `
    If passed to \`npm install\`, will rebuild the npm tree such that only
    versions that were available **on or before** the \`--before\` time get
    installed.  If there's no versions available for the current set of
    direct dependencies, the command will error.

    If the requested version is a \`dist-tag\` and the given tag does not
    pass the \`--before\` filter, the most recent version less than or equal
    to that tag will be used. For example, \`foo@latest\` might install
    \`foo@1.2\` even though \`latest\` is \`2.0\`.
  `,
  flatten,
})

define('bin-links', {
  default: true,
  type: Boolean,
  description: `
    Tells npm to create symlinks (or \`.cmd\` shims on Windows) for package
    executables.

    Set to false to have it not do this.  This can be used to work around the
    fact that some file systems don't support symlinks, even on ostensibly
    Unix systems.
  `,
  flatten,
})

define('browser', {
  default: null,
  defaultDescription: `
    OS X: \`"open"\`, Windows: \`"start"\`, Others: \`"xdg-open"\`
  `,
  type: [null, Boolean, String],
  description: `
    The browser that is called by npm commands to open websites.

    Set to \`false\` to suppress browser behavior and instead print urls to
    terminal.

    Set to \`true\` to use default system URL opener.
  `,
  flatten,
})

define('ca', {
  default: null,
  type: [null, String, Array],
  description: `
    The Certificate Authority signing certificate that is trusted for SSL
    connections to the registry. Values should be in PEM format (Windows
    calls it "Base-64 encoded X.509 (.CER)") with newlines replaced by the
    string "\\n". For example:

    \`\`\`ini
    ca="-----BEGIN CERTIFICATE-----\\nXXXX\\nXXXX\\n-----END CERTIFICATE-----"
    \`\`\`

    Set to \`null\` to only allow "known" registrars, or to a specific CA
    cert to trust only that specific signing authority.

    Multiple CAs can be trusted by specifying an array of certificates:

    \`\`\`ini
    ca[]="..."
    ca[]="..."
    \`\`\`

    See also the \`strict-ssl\` config.
  `,
  flatten,
})

define('cache', {
  default: cache,
  defaultDescription: `
    Windows: \`%LocalAppData%\\npm-cache\`, Posix: \`~/.npm\`
  `,
  type: path,
  description: `
    The location of npm's cache directory.  See [\`npm
    cache\`](/commands/npm-cache)
  `,
  flatten (key, obj, flatOptions) {
    flatOptions.cache = join(obj.cache, '_cacache')
    flatOptions.npxCache = join(obj.cache, '_npx')
  },
})

define('cache-max', {
  default: Infinity,
  type: Number,
  description: `
    \`--cache-max=0\` is an alias for \`--prefer-online\`
  `,
  deprecated: `
    This option has been deprecated in favor of \`--prefer-online\`
  `,
  flatten (key, obj, flatOptions) {
    if (obj[key] <= 0) {
      flatOptions.preferOnline = true
    }
  },
})

define('cache-min', {
  default: 0,
  type: Number,
  description: `
    \`--cache-min=9999 (or bigger)\` is an alias for \`--prefer-offline\`.
  `,
  deprecated: `
    This option has been deprecated in favor of \`--prefer-offline\`.
  `,
  flatten (key, obj, flatOptions) {
    if (obj[key] >= 9999) {
      flatOptions.preferOffline = true
    }
  },
})

define('cafile', {
  default: null,
  type: path,
  description: `
    A path to a file containing one or multiple Certificate Authority signing
    certificates. Similar to the \`ca\` setting, but allows for multiple
    CA's, as well as for the CA information to be stored in a file on disk.
  `,
  flatten (key, obj, flatOptions) {
    // always set to null in defaults
    if (!obj.cafile) {
      return
    }

    const raw = maybeReadFile(obj.cafile)
    if (!raw) {
      return
    }

    const delim = '-----END CERTIFICATE-----'
    flatOptions.ca = raw.replace(/\r\n/g, '\n').split(delim)
      .filter(section => section.trim())
      .map(section => section.trimLeft() + delim)
  },
})

define('call', {
  default: '',
  type: String,
  short: 'c',
  description: `
    Optional companion option for \`npm exec\`, \`npx\` that allows for
    specifying a custom command to be run along with the installed packages.

    \`\`\`bash
    npm exec --package yo --package generator-node --call "yo node"
    \`\`\`
  `,
  flatten,
})

define('cert', {
  default: null,
  type: [null, String],
  description: `
    A client certificate to pass when accessing the registry.  Values should
    be in PEM format (Windows calls it "Base-64 encoded X.509 (.CER)") with
    newlines replaced by the string "\\n". For example:

    \`\`\`ini
    cert="-----BEGIN CERTIFICATE-----\\nXXXX\\nXXXX\\n-----END CERTIFICATE-----"
    \`\`\`

    It is _not_ the path to a certificate file (and there is no "certfile"
    option).
  `,
  flatten,
})

define('ci-name', {
  default: ciName || null,
  defaultDescription: `
    The name of the current CI system, or \`null\` when not on a known CI
    platform.
  `,
  type: [null, String],
  description: `
    The name of a continuous integration system.  If not set explicitly, npm
    will detect the current CI environment using the
    [\`@npmcli/ci-detect\`](http://npm.im/@npmcli/ci-detect) module.
  `,
  flatten,
})

define('cidr', {
  default: null,
  type: [null, String, Array],
  description: `
    This is a list of CIDR address to be used when configuring limited access
    tokens with the \`npm token create\` command.
  `,
  flatten,
})

// This should never be directly used, the flattened value is the derived value
// and is sent to other modules, and is also exposed as `npm.color` for use
// inside npm itself.
define('color', {
  default: !process.env.NO_COLOR || process.env.NO_COLOR === '0',
  usage: '--color|--no-color|--color always',
  defaultDescription: `
    true unless the NO_COLOR environ is set to something other than '0'
  `,
  type: ['always', Boolean],
  description: `
    If false, never shows colors.  If \`"always"\` then always shows colors.
    If true, then only prints color codes for tty file descriptors.
  `,
  flatten (key, obj, flatOptions) {
    flatOptions.color = !obj.color ? false
      : obj.color === 'always' ? true
      : !!process.stdout.isTTY
    flatOptions.logColor = !obj.color ? false
      : obj.color === 'always' ? true
      : !!process.stderr.isTTY
  },
})

define('commit-hooks', {
  default: true,
  type: Boolean,
  description: `
    Run git commit hooks when using the \`npm version\` command.
  `,
  flatten,
})

define('depth', {
  default: null,
  defaultDescription: `
    \`Infinity\` if \`--all\` is set, otherwise \`1\`
  `,
  type: [null, Number],
  description: `
    The depth to go when recursing packages for \`npm ls\`.

    If not set, \`npm ls\` will show only the immediate dependencies of the
    root project.  If \`--all\` is set, then npm will show all dependencies
    by default.
  `,
  flatten,
})

define('description', {
  default: true,
  type: Boolean,
  usage: '--no-description',
  description: `
    Show the description in \`npm search\`
  `,
  flatten (key, obj, flatOptions) {
    flatOptions.search = flatOptions.search || { limit: 20 }
    flatOptions.search[key] = obj[key]
  },
})

define('dev', {
  default: false,
  type: Boolean,
  description: `
    Alias for \`--include=dev\`.
  `,
  deprecated: 'Please use --include=dev instead.',
  flatten (key, obj, flatOptions) {
    definitions.omit.flatten('omit', obj, flatOptions)
  },
})

define('diff', {
  default: [],
  hint: '<pkg-name|spec|version>',
  type: [String, Array],
  description: `
    Define arguments to compare in \`npm diff\`.
  `,
  flatten,
})

define('diff-ignore-all-space', {
  default: false,
  type: Boolean,
  description: `
    Ignore whitespace when comparing lines in \`npm diff\`.
  `,
  flatten,
})

define('diff-name-only', {
  default: false,
  type: Boolean,
  description: `
    Prints only filenames when using \`npm diff\`.
  `,
  flatten,
})

define('diff-no-prefix', {
  default: false,
  type: Boolean,
  description: `
    Do not show any source or destination prefix in \`npm diff\` output.

    Note: this causes \`npm diff\` to ignore the \`--diff-src-prefix\` and
    \`--diff-dst-prefix\` configs.
  `,
  flatten,
})

define('diff-dst-prefix', {
  default: 'b/',
  hint: '<path>',
  type: String,
  description: `
    Destination prefix to be used in \`npm diff\` output.
  `,
  flatten,
})

define('diff-src-prefix', {
  default: 'a/',
  hint: '<path>',
  type: String,
  description: `
    Source prefix to be used in \`npm diff\` output.
  `,
  flatten,
})

define('diff-text', {
  default: false,
  type: Boolean,
  description: `
    Treat all files as text in \`npm diff\`.
  `,
  flatten,
})

define('diff-unified', {
  default: 3,
  type: Number,
  description: `
    The number of lines of context to print in \`npm diff\`.
  `,
  flatten,
})

define('dry-run', {
  default: false,
  type: Boolean,
  description: `
    Indicates that you don't want npm to make any changes and that it should
    only report what it would have done.  This can be passed into any of the
    commands that modify your local installation, eg, \`install\`,
    \`update\`, \`dedupe\`, \`uninstall\`, as well as \`pack\` and
    \`publish\`.

    Note: This is NOT honored by other network related commands, eg
    \`dist-tags\`, \`owner\`, etc.
  `,
  flatten,
})

define('editor', {
  default: editor,
  defaultDescription: `
    The EDITOR or VISUAL environment variables, or 'notepad.exe' on Windows,
    or 'vim' on Unix systems
  `,
  type: String,
  description: `
    The command to run for \`npm edit\` and \`npm config edit\`.
  `,
  flatten,
})

define('engine-strict', {
  default: false,
  type: Boolean,
  description: `
    If set to true, then npm will stubbornly refuse to install (or even
    consider installing) any package that claims to not be compatible with
    the current Node.js version.

    This can be overridden by setting the \`--force\` flag.
  `,
  flatten,
})

define('fetch-retries', {
  default: 2,
  type: Number,
  description: `
    The "retries" config for the \`retry\` module to use when fetching
    packages from the registry.

    npm will retry idempotent read requests to the registry in the case
    of network failures or 5xx HTTP errors.
  `,
  flatten (key, obj, flatOptions) {
    flatOptions.retry = flatOptions.retry || {}
    flatOptions.retry.retries = obj[key]
  },
})

define('fetch-retry-factor', {
  default: 10,
  type: Number,
  description: `
    The "factor" config for the \`retry\` module to use when fetching
    packages.
  `,
  flatten (key, obj, flatOptions) {
    flatOptions.retry = flatOptions.retry || {}
    flatOptions.retry.factor = obj[key]
  },
})

define('fetch-retry-maxtimeout', {
  default: 60000,
  defaultDescription: '60000 (1 minute)',
  type: Number,
  description: `
    The "maxTimeout" config for the \`retry\` module to use when fetching
    packages.
  `,
  flatten (key, obj, flatOptions) {
    flatOptions.retry = flatOptions.retry || {}
    flatOptions.retry.maxTimeout = obj[key]
  },
})

define('fetch-retry-mintimeout', {
  default: 10000,
  defaultDescription: '10000 (10 seconds)',
  type: Number,
  description: `
    The "minTimeout" config for the \`retry\` module to use when fetching
    packages.
  `,
  flatten (key, obj, flatOptions) {
    flatOptions.retry = flatOptions.retry || {}
    flatOptions.retry.minTimeout = obj[key]
  },
})

define('fetch-timeout', {
  default: 5 * 60 * 1000,
  defaultDescription: `${5 * 60 * 1000} (5 minutes)`,
  type: Number,
  description: `
    The maximum amount of time to wait for HTTP requests to complete.
  `,
  flatten (key, obj, flatOptions) {
    flatOptions.timeout = obj[key]
  },
})

define('force', {
  default: false,
  type: Boolean,
  short: 'f',
  description: `
    Removes various protections against unfortunate side effects, common
    mistakes, unnecessary performance degradation, and malicious input.

    * Allow clobbering non-npm files in global installs.
    * Allow the \`npm version\` command to work on an unclean git repository.
    * Allow deleting the cache folder with \`npm cache clean\`.
    * Allow installing packages that have an \`engines\` declaration
      requiring a different version of npm.
    * Allow installing packages that have an \`engines\` declaration
      requiring a different version of \`node\`, even if \`--engine-strict\`
      is enabled.
    * Allow \`npm audit fix\` to install modules outside your stated
      dependency range (including SemVer-major changes).
    * Allow unpublishing all versions of a published package.
    * Allow conflicting peerDependencies to be installed in the root project.
    * Implicitly set \`--yes\` during \`npm init\`.
    * Allow clobbering existing values in \`npm pkg\`
    * Allow unpublishing of entire packages (not just a single version).

    If you don't have a clear idea of what you want to do, it is strongly
    recommended that you do not use this option!
  `,
  flatten,
})

define('foreground-scripts', {
  default: false,
  type: Boolean,
  description: `
    Run all build scripts (ie, \`preinstall\`, \`install\`, and
    \`postinstall\`) scripts for installed packages in the foreground
    process, sharing standard input, output, and error with the main npm
    process.

    Note that this will generally make installs run slower, and be much
    noisier, but can be useful for debugging.
  `,
  flatten,
})

define('format-package-lock', {
  default: true,
  type: Boolean,
  description: `
    Format \`package-lock.json\` or \`npm-shrinkwrap.json\` as a human
    readable file.
  `,
  flatten,
})

define('fund', {
  default: true,
  type: Boolean,
  description: `
    When "true" displays the message at the end of each \`npm install\`
    acknowledging the number of dependencies looking for funding.
    See [\`npm fund\`](/commands/npm-fund) for details.
  `,
  flatten,
})

define('git', {
  default: 'git',
  type: String,
  description: `
    The command to use for git commands.  If git is installed on the
    computer, but is not in the \`PATH\`, then set this to the full path to
    the git binary.
  `,
  flatten,
})

define('git-tag-version', {
  default: true,
  type: Boolean,
  description: `
    Tag the commit when using the \`npm version\` command.  Setting this to
    false results in no commit being made at all.
  `,
  flatten,
})

define('global', {
  default: false,
  type: Boolean,
  short: 'g',
  deprecated: `
    \`--global\`, \`--local\` are deprecated. Use \`--location=global\` instead.
  `,
  description: `
    Operates in "global" mode, so that packages are installed into the
    \`prefix\` folder instead of the current working directory.  See
    [folders](/configuring-npm/folders) for more on the differences in
    behavior.

    * packages are installed into the \`{prefix}/lib/node_modules\` folder,
      instead of the current working directory.
    * bin files are linked to \`{prefix}/bin\`
    * man pages are linked to \`{prefix}/share/man\`
  `,
  flatten: (key, obj, flatOptions) => {
    flatten(key, obj, flatOptions)
    if (flatOptions.global) {
      flatOptions.location = 'global'
    }
  },
})

define('global-style', {
  default: false,
  type: Boolean,
  description: `
    Causes npm to install the package into your local \`node_modules\` folder
    with the same layout it uses with the global \`node_modules\` folder.
    Only your direct dependencies will show in \`node_modules\` and
    everything they depend on will be flattened in their \`node_modules\`
    folders.  This obviously will eliminate some deduping. If used with
    \`legacy-bundling\`, \`legacy-bundling\` will be preferred.
  `,
  flatten,
})

// the globalconfig has its default defined outside of this module
define('globalconfig', {
  type: path,
  default: '',
  defaultDescription: `
    The global --prefix setting plus 'etc/npmrc'. For example,
    '/usr/local/etc/npmrc'
  `,
  description: `
    The config file to read for global config options.
  `,
  flatten,
})

define('heading', {
  default: 'npm',
  type: String,
  description: `
    The string that starts all the debugging log output.
  `,
  flatten,
})

define('https-proxy', {
  default: null,
  type: [null, url],
  description: `
    A proxy to use for outgoing https requests. If the \`HTTPS_PROXY\` or
    \`https_proxy\` or \`HTTP_PROXY\` or \`http_proxy\` environment variables
    are set, proxy settings will be honored by the underlying
    \`make-fetch-happen\` library.
  `,
  flatten,
})

define('if-present', {
  default: false,
  type: Boolean,
  envExport: false,
  description: `
    If true, npm will not exit with an error code when \`run-script\` is
    invoked for a script that isn't defined in the \`scripts\` section of
    \`package.json\`. This option can be used when it's desirable to
    optionally run a script when it's present and fail if the script fails.
    This is useful, for example, when running scripts that may only apply for
    some builds in an otherwise generic CI setup.
  `,
  flatten,
})

define('ignore-scripts', {
  default: false,
  type: Boolean,
  description: `
    If true, npm does not run scripts specified in package.json files.

    Note that commands explicitly intended to run a particular script, such
    as \`npm start\`, \`npm stop\`, \`npm restart\`, \`npm test\`, and \`npm
    run-script\` will still run their intended script if \`ignore-scripts\` is
    set, but they will *not* run any pre- or post-scripts.
  `,
  flatten,
})

define('include', {
  default: [],
  type: [Array, 'prod', 'dev', 'optional', 'peer'],
  description: `
    Option that allows for defining which types of dependencies to install.

    This is the inverse of \`--omit=<type>\`.

    Dependency types specified in \`--include\` will not be omitted,
    regardless of the order in which omit/include are specified on the
    command-line.
  `,
  flatten (key, obj, flatOptions) {
    // just call the omit flattener, it reads from obj.include
    definitions.omit.flatten('omit', obj, flatOptions)
  },
})

define('include-staged', {
  default: false,
  type: Boolean,
  description: `
    Allow installing "staged" published packages, as defined by [npm RFC PR
    #92](https://github.com/npm/rfcs/pull/92).

    This is experimental, and not implemented by the npm public registry.
  `,
  flatten,
})

define('include-workspace-root', {
  default: false,
  type: Boolean,
  envExport: false,
  description: `
    Include the workspace root when workspaces are enabled for a command.

    When false, specifying individual workspaces via the \`workspace\` config,
    or all workspaces via the \`workspaces\` flag, will cause npm to operate only
    on the specified workspaces, and not on the root project.
  `,
  flatten,
})

define('init-author-email', {
  default: '',
  type: String,
  description: `
    The value \`npm init\` should use by default for the package author's
    email.
  `,
})

define('init-author-name', {
  default: '',
  type: String,
  description: `
    The value \`npm init\` should use by default for the package author's name.
  `,
})

define('init-author-url', {
  default: '',
  type: ['', url],
  description: `
    The value \`npm init\` should use by default for the package author's homepage.
  `,
})

define('init-license', {
  default: 'ISC',
  type: String,
  description: `
    The value \`npm init\` should use by default for the package license.
  `,
})

define('init-module', {
  default: '~/.npm-init.js',
  type: path,
  description: `
    A module that will be loaded by the \`npm init\` command.  See the
    documentation for the
    [init-package-json](https://github.com/npm/init-package-json) module for
    more information, or [npm init](/commands/npm-init).
  `,
})

define('init-version', {
  default: '1.0.0',
  type: semver,
  description: `
    The value that \`npm init\` should use by default for the package
    version number, if not already set in package.json.
  `,
})

// these "aliases" are historically supported in .npmrc files, unfortunately
// They should be removed in a future npm version.
define('init.author.email', {
  default: '',
  type: String,
  deprecated: `
    Use \`--init-author-email\` instead.`,
  description: `
    Alias for \`--init-author-email\`
  `,
})

define('init.author.name', {
  default: '',
  type: String,
  deprecated: `
    Use \`--init-author-name\` instead.
  `,
  description: `
    Alias for \`--init-author-name\`
  `,
})

define('init.author.url', {
  default: '',
  type: ['', url],
  deprecated: `
    Use \`--init-author-url\` instead.
  `,
  description: `
    Alias for \`--init-author-url\`
  `,
})

define('init.license', {
  default: 'ISC',
  type: String,
  deprecated: `
    Use \`--init-license\` instead.
  `,
  description: `
    Alias for \`--init-license\`
  `,
})

define('init.module', {
  default: '~/.npm-init.js',
  type: path,
  deprecated: `
    Use \`--init-module\` instead.
  `,
  description: `
    Alias for \`--init-module\`
  `,
})

define('init.version', {
  default: '1.0.0',
  type: semver,
  deprecated: `
    Use \`--init-version\` instead.
  `,
  description: `
    Alias for \`--init-version\`
  `,
})

define('install-links', {
  default: false,
  type: Boolean,
  description: `
    When set file: protocol dependencies that exist outside of the project root
    will be packed and installed as regular dependencies instead of creating a
    symlink. This option has no effect on workspaces.
  `,
  flatten,
})

define('json', {
  default: false,
  type: Boolean,
  description: `
    Whether or not to output JSON data, rather than the normal output.

    * In \`npm pkg set\` it enables parsing set values with JSON.parse()
    before saving them to your \`package.json\`.

    Not supported by all npm commands.
  `,
  flatten,
})

define('key', {
  default: null,
  type: [null, String],
  description: `
    A client key to pass when accessing the registry.  Values should be in
    PEM format with newlines replaced by the string "\\n". For example:

    \`\`\`ini
    key="-----BEGIN PRIVATE KEY-----\\nXXXX\\nXXXX\\n-----END PRIVATE KEY-----"
    \`\`\`

    It is _not_ the path to a key file (and there is no "keyfile" option).
  `,
  flatten,
})

define('legacy-bundling', {
  default: false,
  type: Boolean,
  description: `
    Causes npm to install the package such that versions of npm prior to 1.4,
    such as the one included with node 0.8, can install the package.  This
    eliminates all automatic deduping. If used with \`global-style\` this
    option will be preferred.
  `,
  flatten,
})

define('legacy-peer-deps', {
  default: false,
  type: Boolean,
  description: `
    Causes npm to completely ignore \`peerDependencies\` when building a
    package tree, as in npm versions 3 through 6.

    If a package cannot be installed because of overly strict
    \`peerDependencies\` that collide, it provides a way to move forward
    resolving the situation.

    This differs from \`--omit=peer\`, in that \`--omit=peer\` will avoid
    unpacking \`peerDependencies\` on disk, but will still design a tree such
    that \`peerDependencies\` _could_ be unpacked in a correct place.

    Use of \`legacy-peer-deps\` is not recommended, as it will not enforce
    the \`peerDependencies\` contract that meta-dependencies may rely on.
  `,
  flatten,
})

define('link', {
  default: false,
  type: Boolean,
  description: `
    Used with \`npm ls\`, limiting output to only those packages that are
    linked.
  `,
})

define('local-address', {
  default: null,
  type: getLocalAddresses(),
  typeDescription: 'IP Address',
  description: `
    The IP address of the local interface to use when making connections to
    the npm registry.  Must be IPv4 in versions of Node prior to 0.12.
  `,
  flatten,
})

define('location', {
  default: 'user',
  short: 'L',
  type: [
    'global',
    'user',
    'project',
  ],
  defaultDescription: `
    "user" unless \`--global\` is passed, which will also set this value to "global"
  `,
  description: `
    When passed to \`npm config\` this refers to which config file to use.

    When set to "global" mode, packages are installed into the \`prefix\` folder
    instead of the current working directory. See
    [folders](/configuring-npm/folders) for more on the differences in behavior.

    * packages are installed into the \`{prefix}/lib/node_modules\` folder,
      instead of the current working directory.
    * bin files are linked to \`{prefix}/bin\`
    * man pages are linked to \`{prefix}/share/man\`
  `,
  flatten: (key, obj, flatOptions) => {
    flatten(key, obj, flatOptions)
    if (flatOptions.global) {
      flatOptions.location = 'global'
    }
    if (obj.location === 'global') {
      flatOptions.global = true
    }
  },
})

define('lockfile-version', {
  default: null,
  type: [null, 1, 2, 3, '1', '2', '3'],
  defaultDescription: `
    Version 2 if no lockfile or current lockfile version less than or equal to
    2, otherwise maintain current lockfile version
  `,
  description: `
    Set the lockfile format version to be used in package-lock.json and
    npm-shrinkwrap-json files.  Possible options are:

    1: The lockfile version used by npm versions 5 and 6.  Lacks some data that
    is used during the install, resulting in slower and possibly less
    deterministic installs.  Prevents lockfile churn when interoperating with
    older npm versions.

    2: The default lockfile version used by npm version 7.  Includes both the
    version 1 lockfile data and version 3 lockfile data, for maximum
    determinism and interoperability, at the expense of more bytes on disk.

    3: Only the new lockfile information introduced in npm version 7.  Smaller
    on disk than lockfile version 2, but not interoperable with older npm
    versions.  Ideal if all users are on npm version 7 and higher.
  `,
  flatten: (key, obj, flatOptions) => {
    flatOptions.lockfileVersion = obj[key] && parseInt(obj[key], 10)
  },
})

define('loglevel', {
  default: 'notice',
  type: [
    'silent',
    'error',
    'warn',
    'notice',
    'http',
    'timing',
    'info',
    'verbose',
    'silly',
  ],
  description: `
    What level of logs to report.  All logs are written to a debug log,
    with the path to that file printed if the execution of a command fails.

    Any logs of a higher level than the setting are shown. The default is
    "notice".

    See also the \`foreground-scripts\` config.
  `,
  flatten (key, obj, flatOptions) {
    flatOptions.silent = obj[key] === 'silent'
  },
})

define('logs-dir', {
  default: null,
  type: [null, path],
  defaultDescription: `
    A directory named \`_logs\` inside the cache
`,
  description: `
    The location of npm's log directory.  See [\`npm
    logging\`](/using-npm/logging) for more information.
  `,
})

define('logs-max', {
  default: 10,
  type: Number,
  description: `
    The maximum number of log files to store.

    If set to 0, no log files will be written for the current run.
  `,
})

define('long', {
  default: false,
  type: Boolean,
  short: 'l',
  description: `
    Show extended information in \`ls\`, \`search\`, and \`help-search\`.
  `,
})

define('maxsockets', {
  default: 15,
  type: Number,
  description: `
    The maximum number of connections to use per origin (protocol/host/port
    combination).
  `,
  flatten (key, obj, flatOptions) {
    flatOptions.maxSockets = obj[key]
  },
})

define('message', {
  default: '%s',
  type: String,
  short: 'm',
  description: `
    Commit message which is used by \`npm version\` when creating version commit.

    Any "%s" in the message will be replaced with the version number.
  `,
  flatten,
})

define('node-options', {
  default: null,
  type: [null, String],
  description: `
    Options to pass through to Node.js via the \`NODE_OPTIONS\` environment
    variable.  This does not impact how npm itself is executed but it does
    impact how lifecycle scripts are called.
  `,
})

define('node-version', {
  default: process.version,
  defaultDescription: 'Node.js `process.version` value',
  type: semver,
  description: `
    The node version to use when checking a package's \`engines\` setting.
  `,
  flatten,
})

define('noproxy', {
  default: '',
  defaultDescription: `
    The value of the NO_PROXY environment variable
  `,
  type: [String, Array],
  description: `
    Domain extensions that should bypass any proxies.

    Also accepts a comma-delimited string.
  `,
  flatten (key, obj, flatOptions) {
    if (Array.isArray(obj[key])) {
      flatOptions.noProxy = obj[key].join(',')
    } else {
      flatOptions.noProxy = obj[key]
    }
  },
})

define('npm-version', {
  default: npmVersion,
  defaultDescription: 'Output of `npm --version`',
  type: semver,
  description: `
    The npm version to use when checking a package's \`engines\` setting.
  `,
  flatten,
})

define('offline', {
  default: false,
  type: Boolean,
  description: `
    Force offline mode: no network requests will be done during install. To allow
    the CLI to fill in missing cache data, see \`--prefer-offline\`.
  `,
  flatten,
})

define('omit', {
  default: process.env.NODE_ENV === 'production' ? ['dev'] : [],
  defaultDescription: `
    'dev' if the \`NODE_ENV\` environment variable is set to 'production',
    otherwise empty.
  `,
  type: [Array, 'dev', 'optional', 'peer'],
  description: `
    Dependency types to omit from the installation tree on disk.

    Note that these dependencies _are_ still resolved and added to the
    \`package-lock.json\` or \`npm-shrinkwrap.json\` file.  They are just
    not physically installed on disk.

    If a package type appears in both the \`--include\` and \`--omit\`
    lists, then it will be included.

    If the resulting omit list includes \`'dev'\`, then the \`NODE_ENV\`
    environment variable will be set to \`'production'\` for all lifecycle
    scripts.
  `,
  flatten (key, obj, flatOptions) {
    flatOptions.omit = buildOmitList(obj)
  },
})

define('omit-lockfile-registry-resolved', {
  default: false,
  type: Boolean,
  description: `
    This option causes npm to create lock files without a \`resolved\` key for
    registry dependencies. Subsequent installs will need to resolve tarball
    endpoints with the configured registry, likely resulting in a longer install
    time.
  `,
  flatten,
})

define('only', {
  default: null,
  type: [null, 'prod', 'production'],
  deprecated: `
    Use \`--omit=dev\` to omit dev dependencies from the install.
  `,
  description: `
    When set to \`prod\` or \`production\`, this is an alias for
    \`--omit=dev\`.
  `,
  flatten (key, obj, flatOptions) {
    definitions.omit.flatten('omit', obj, flatOptions)
  },
})

define('optional', {
  default: null,
  type: [null, Boolean],
  deprecated: `
    Use \`--omit=optional\` to exclude optional dependencies, or
    \`--include=optional\` to include them.

    Default value does install optional deps unless otherwise omitted.
  `,
  description: `
    Alias for --include=optional or --omit=optional
  `,
  flatten (key, obj, flatOptions) {
    definitions.omit.flatten('omit', obj, flatOptions)
  },
})

define('otp', {
  default: null,
  type: [null, String],
  description: `
    This is a one-time password from a two-factor authenticator.  It's needed
    when publishing or changing package permissions with \`npm access\`.

    If not set, and a registry response fails with a challenge for a one-time
    password, npm will prompt on the command line for one.
  `,
  flatten,
})

define('package', {
  default: [],
  hint: '<pkg>[@<version>]',
  type: [String, Array],
  description: `
    The package to install for [\`npm exec\`](/commands/npm-exec)
  `,
  flatten,
})

define('package-lock', {
  default: true,
  type: Boolean,
  description: `
    If set to false, then ignore \`package-lock.json\` files when installing.
    This will also prevent _writing_ \`package-lock.json\` if \`save\` is
    true.

    This configuration does not affect \`npm ci\`.
  `,
  flatten: (key, obj, flatOptions) => {
    flatten(key, obj, flatOptions)
    if (flatOptions.packageLockOnly) {
      flatOptions.packageLock = true
    }
  },
})

define('package-lock-only', {
  default: false,
  type: Boolean,
  description: `
    If set to true, the current operation will only use the \`package-lock.json\`,
    ignoring \`node_modules\`.

    For \`update\` this means only the \`package-lock.json\` will be updated,
    instead of checking \`node_modules\` and downloading dependencies.

    For \`list\` this means the output will be based on the tree described by the
    \`package-lock.json\`, rather than the contents of \`node_modules\`.
  `,
  flatten: (key, obj, flatOptions) => {
    flatten(key, obj, flatOptions)
    if (flatOptions.packageLockOnly) {
      flatOptions.packageLock = true
    }
  },
})

define('pack-destination', {
  default: '.',
  type: String,
  description: `
    Directory in which \`npm pack\` will save tarballs.
  `,
  flatten,
})

define('parseable', {
  default: false,
  type: Boolean,
  short: 'p',
  description: `
    Output parseable results from commands that write to standard output. For
    \`npm search\`, this will be tab-separated table format.
  `,
  flatten,
})

define('prefer-offline', {
  default: false,
  type: Boolean,
  description: `
    If true, staleness checks for cached data will be bypassed, but missing
    data will be requested from the server. To force full offline mode, use
    \`--offline\`.
  `,
  flatten,
})

define('prefer-online', {
  default: false,
  type: Boolean,
  description: `
    If true, staleness checks for cached data will be forced, making the CLI
    look for updates immediately even for fresh package data.
  `,
  flatten,
})

// `prefix` has its default defined outside of this module
define('prefix', {
  type: path,
  short: 'C',
  default: '',
  defaultDescription: `
    In global mode, the folder where the node executable is installed. In
    local mode, the nearest parent folder containing either a package.json
    file or a node_modules folder.
  `,
  description: `
    The location to install global items.  If set on the command line, then
    it forces non-global commands to run in the specified folder.
  `,
})

define('preid', {
  default: '',
  hint: 'prerelease-id',
  type: String,
  description: `
    The "prerelease identifier" to use as a prefix for the "prerelease" part
    of a semver. Like the \`rc\` in \`1.2.0-rc.8\`.
  `,
  flatten,
})

define('production', {
  default: null,
  type: [null, Boolean],
  deprecated: 'Use `--omit=dev` instead.',
  description: 'Alias for `--omit=dev`',
  flatten (key, obj, flatOptions) {
    definitions.omit.flatten('omit', obj, flatOptions)
  },
})

define('progress', {
  default: !ciName,
  defaultDescription: `
    \`true\` unless running in a known CI system
  `,
  type: Boolean,
  description: `
    When set to \`true\`, npm will display a progress bar during time
    intensive operations, if \`process.stderr\` is a TTY.

    Set to \`false\` to suppress the progress bar.
  `,
  flatten (key, obj, flatOptions) {
    flatOptions.progress = !obj.progress ? false
      : !!process.stderr.isTTY && process.env.TERM !== 'dumb'
  },
})

define('proxy', {
  default: null,
  type: [null, false, url], // allow proxy to be disabled explicitly
  description: `
    A proxy to use for outgoing http requests. If the \`HTTP_PROXY\` or
    \`http_proxy\` environment variables are set, proxy settings will be
    honored by the underlying \`request\` library.
  `,
  flatten,
})

define('read-only', {
  default: false,
  type: Boolean,
  description: `
    This is used to mark a token as unable to publish when configuring
    limited access tokens with the \`npm token create\` command.
  `,
  flatten,
})

define('rebuild-bundle', {
  default: true,
  type: Boolean,
  description: `
    Rebuild bundled dependencies after installation.
  `,
  flatten,
})

define('registry', {
  default: 'https://registry.npmjs.org/',
  type: url,
  description: `
    The base URL of the npm registry.
  `,
  flatten,
})

define('save', {
  default: true,
  defaultDescription: `\`true\` unless when using \`npm update\` where it
  defaults to \`false\``,
  usage: '-S|--save|--no-save|--save-prod|--save-dev|--save-optional|--save-peer|--save-bundle',
  type: Boolean,
  short: 'S',
  description: `
    Save installed packages to a \`package.json\` file as dependencies.

    When used with the \`npm rm\` command, removes the dependency from
    \`package.json\`.

    Will also prevent writing to \`package-lock.json\` if set to \`false\`.
  `,
  flatten,
})

define('save-bundle', {
  default: false,
  type: Boolean,
  short: 'B',
  description: `
    If a package would be saved at install time by the use of \`--save\`,
    \`--save-dev\`, or \`--save-optional\`, then also put it in the
    \`bundleDependencies\` list.

    Ignored if \`--save-peer\` is set, since peerDependencies cannot be bundled.
  `,
  flatten (key, obj, flatOptions) {
    // XXX update arborist to just ignore it if resulting saveType is peer
    // otherwise this won't have the expected effect:
    //
    // npm config set save-peer true
    // npm i foo --save-bundle --save-prod <-- should bundle
    flatOptions.saveBundle = obj['save-bundle'] && !obj['save-peer']
  },
})

// XXX: We should really deprecate all these `--save-blah` switches
// in favor of a single `--save-type` option.  The unfortunate shortcut
// we took for `--save-peer --save-optional` being `--save-type=peerOptional`
// makes this tricky, and likely a breaking change.

define('save-dev', {
  default: false,
  type: Boolean,
  short: 'D',
  description: `
    Save installed packages to a package.json file as \`devDependencies\`.
  `,
  flatten (key, obj, flatOptions) {
    if (!obj[key]) {
      if (flatOptions.saveType === 'dev') {
        delete flatOptions.saveType
      }
      return
    }

    flatOptions.saveType = 'dev'
  },
})

define('save-exact', {
  default: false,
  type: Boolean,
  short: 'E',
  description: `
    Dependencies saved to package.json will be configured with an exact
    version rather than using npm's default semver range operator.
  `,
  flatten (key, obj, flatOptions) {
    // just call the save-prefix flattener, it reads from obj['save-exact']
    definitions['save-prefix'].flatten('save-prefix', obj, flatOptions)
  },
})

define('save-optional', {
  default: false,
  type: Boolean,
  short: 'O',
  description: `
    Save installed packages to a package.json file as
    \`optionalDependencies\`.
  `,
  flatten (key, obj, flatOptions) {
    if (!obj[key]) {
      if (flatOptions.saveType === 'optional') {
        delete flatOptions.saveType
      } else if (flatOptions.saveType === 'peerOptional') {
        flatOptions.saveType = 'peer'
      }
      return
    }

    if (flatOptions.saveType === 'peerOptional') {
      return
    }

    if (flatOptions.saveType === 'peer') {
      flatOptions.saveType = 'peerOptional'
    } else {
      flatOptions.saveType = 'optional'
    }
  },
})

define('save-peer', {
  default: false,
  type: Boolean,
  description: `
    Save installed packages to a package.json file as \`peerDependencies\`
  `,
  flatten (key, obj, flatOptions) {
    if (!obj[key]) {
      if (flatOptions.saveType === 'peer') {
        delete flatOptions.saveType
      } else if (flatOptions.saveType === 'peerOptional') {
        flatOptions.saveType = 'optional'
      }
      return
    }

    if (flatOptions.saveType === 'peerOptional') {
      return
    }

    if (flatOptions.saveType === 'optional') {
      flatOptions.saveType = 'peerOptional'
    } else {
      flatOptions.saveType = 'peer'
    }
  },
})

define('save-prefix', {
  default: '^',
  type: String,
  description: `
    Configure how versions of packages installed to a package.json file via
    \`--save\` or \`--save-dev\` get prefixed.

    For example if a package has version \`1.2.3\`, by default its version is
    set to \`^1.2.3\` which allows minor upgrades for that package, but after
    \`npm config set save-prefix='~'\` it would be set to \`~1.2.3\` which
    only allows patch upgrades.
  `,
  flatten (key, obj, flatOptions) {
    flatOptions.savePrefix = obj['save-exact'] ? '' : obj['save-prefix']
    obj['save-prefix'] = flatOptions.savePrefix
  },
})

define('save-prod', {
  default: false,
  type: Boolean,
  short: 'P',
  description: `
    Save installed packages into \`dependencies\` specifically. This is
    useful if a package already exists in \`devDependencies\` or
    \`optionalDependencies\`, but you want to move it to be a non-optional
    production dependency.

    This is the default behavior if \`--save\` is true, and neither
    \`--save-dev\` or \`--save-optional\` are true.
  `,
  flatten (key, obj, flatOptions) {
    if (!obj[key]) {
      if (flatOptions.saveType === 'prod') {
        delete flatOptions.saveType
      }
      return
    }

    flatOptions.saveType = 'prod'
  },
})

define('scope', {
  default: '',
  defaultDescription: `
    the scope of the current project, if any, or ""
  `,
  type: String,
  hint: '<@scope>',
  description: `
    Associate an operation with a scope for a scoped registry.

    Useful when logging in to or out of a private registry:

    \`\`\`
    # log in, linking the scope to the custom registry
    npm login --scope=@mycorp --registry=https://registry.mycorp.com

    # log out, removing the link and the auth token
    npm logout --scope=@mycorp
    \`\`\`

    This will cause \`@mycorp\` to be mapped to the registry for future
    installation of packages specified according to the pattern
    \`@mycorp/package\`.

    This will also cause \`npm init\` to create a scoped package.

    \`\`\`
    # accept all defaults, and create a package named "@foo/whatever",
    # instead of just named "whatever"
    npm init --scope=@foo --yes
    \`\`\`
  `,
  flatten (key, obj, flatOptions) {
    const value = obj[key]
    const scope = value && !/^@/.test(value) ? `@${value}` : value
    flatOptions.scope = scope
    // projectScope is kept for compatibility with npm-registry-fetch
    flatOptions.projectScope = scope
  },
})

define('script-shell', {
  default: null,
  defaultDescription: `
    '/bin/sh' on POSIX systems, 'cmd.exe' on Windows
  `,
  type: [null, String],
  description: `
    The shell to use for scripts run with the \`npm exec\`,
    \`npm run\` and \`npm init <pkg>\` commands.
  `,
  flatten (key, obj, flatOptions) {
    flatOptions.scriptShell = obj[key] || undefined
  },
})

define('searchexclude', {
  default: '',
  type: String,
  description: `
    Space-separated options that limit the results from search.
  `,
  flatten (key, obj, flatOptions) {
    flatOptions.search = flatOptions.search || { limit: 20 }
    flatOptions.search.exclude = obj[key].toLowerCase()
  },
})

define('searchlimit', {
  default: 20,
  type: Number,
  description: `
    Number of items to limit search results to. Will not apply at all to
    legacy searches.
  `,
  flatten (key, obj, flatOptions) {
    flatOptions.search = flatOptions.search || {}
    flatOptions.search.limit = obj[key]
  },
})

define('searchopts', {
  default: '',
  type: String,
  description: `
    Space-separated options that are always passed to search.
  `,
  flatten (key, obj, flatOptions) {
    flatOptions.search = flatOptions.search || { limit: 20 }
    flatOptions.search.opts = querystring.parse(obj[key])
  },
})

define('searchstaleness', {
  default: 15 * 60,
  type: Number,
  description: `
    The age of the cache, in seconds, before another registry request is made
    if using legacy search endpoint.
  `,
  flatten (key, obj, flatOptions) {
    flatOptions.search = flatOptions.search || { limit: 20 }
    flatOptions.search.staleness = obj[key]
  },
})

define('shell', {
  default: shell,
  defaultDescription: `
    SHELL environment variable, or "bash" on Posix, or "cmd.exe" on Windows
  `,
  type: String,
  description: `
    The shell to run for the \`npm explore\` command.
  `,
  flatten,
})

define('shrinkwrap', {
  default: true,
  type: Boolean,
  deprecated: `
    Use the --package-lock setting instead.
  `,
  description: `
    Alias for --package-lock
  `,
  flatten (key, obj, flatOptions) {
    obj['package-lock'] = obj.shrinkwrap
    definitions['package-lock'].flatten('package-lock', obj, flatOptions)
  },
})

define('sign-git-commit', {
  default: false,
  type: Boolean,
  description: `
    If set to true, then the \`npm version\` command will commit the new
    package version using \`-S\` to add a signature.

    Note that git requires you to have set up GPG keys in your git configs
    for this to work properly.
  `,
  flatten,
})

define('sign-git-tag', {
  default: false,
  type: Boolean,
  description: `
    If set to true, then the \`npm version\` command will tag the version
    using \`-s\` to add a signature.

    Note that git requires you to have set up GPG keys in your git configs
    for this to work properly.
  `,
  flatten,
})

define('sso-poll-frequency', {
  default: 500,
  type: Number,
  deprecated: `
    The --auth-type method of SSO/SAML/OAuth will be removed in a future
    version of npm in favor of web-based login.
  `,
  description: `
    When used with SSO-enabled \`auth-type\`s, configures how regularly the
    registry should be polled while the user is completing authentication.
  `,
  flatten,
})

define('sso-type', {
  default: 'oauth',
  type: [null, 'oauth', 'saml'],
  deprecated: `
    The --auth-type method of SSO/SAML/OAuth will be removed in a future
    version of npm in favor of web-based login.
  `,
  description: `
    If \`--auth-type=sso\`, the type of SSO type to use.
  `,
  flatten,
})

define('strict-peer-deps', {
  default: false,
  type: Boolean,
  description: `
    If set to \`true\`, and \`--legacy-peer-deps\` is not set, then _any_
    conflicting \`peerDependencies\` will be treated as an install failure,
    even if npm could reasonably guess the appropriate resolution based on
    non-peer dependency relationships.

    By default, conflicting \`peerDependencies\` deep in the dependency graph
    will be resolved using the nearest non-peer dependency specification,
    even if doing so will result in some packages receiving a peer dependency
    outside the range set in their package's \`peerDependencies\` object.

    When such and override is performed, a warning is printed, explaining the
    conflict and the packages involved.  If \`--strict-peer-deps\` is set,
    then this warning is treated as a failure.
  `,
  flatten,
})

define('strict-ssl', {
  default: true,
  type: Boolean,
  description: `
    Whether or not to do SSL key validation when making requests to the
    registry via https.

    See also the \`ca\` config.
  `,
  flatten (key, obj, flatOptions) {
    flatOptions.strictSSL = obj[key]
  },
})

define('tag', {
  default: 'latest',
  type: String,
  description: `
    If you ask npm to install a package and don't tell it a specific version,
    then it will install the specified tag.

    Also the tag that is added to the package@version specified by the \`npm
    tag\` command, if no explicit tag is given.

    When used by the \`npm diff\` command, this is the tag used to fetch the
    tarball that will be compared with the local files by default.
  `,
  flatten (key, obj, flatOptions) {
    flatOptions.defaultTag = obj[key]
  },
})

define('tag-version-prefix', {
  default: 'v',
  type: String,
  description: `
    If set, alters the prefix used when tagging a new version when performing
    a version increment using  \`npm-version\`. To remove the prefix
    altogether, set it to the empty string: \`""\`.

    Because other tools may rely on the convention that npm version tags look
    like \`v1.0.0\`, _only use this property if it is absolutely necessary_.
    In particular, use care when overriding this setting for public packages.
  `,
  flatten,
})

define('timing', {
  default: false,
  type: Boolean,
  description: `
    If true, writes a debug log to \`logs-dir\` and timing information
    to \`_timing.json\` in the cache, even if the command completes
    successfully.  \`_timing.json\` is a newline delimited list of JSON
    objects.

    You can quickly view it with this [json](https://npm.im/json) command
    line: \`npm exec -- json -g < ~/.npm/_timing.json\`.
  `,
})

define('tmp', {
  default: tmpdir(),
  defaultDescription: `
    The value returned by the Node.js \`os.tmpdir()\` method
    <https://nodejs.org/api/os.html#os_os_tmpdir>
  `,
  type: path,
  deprecated: `
    This setting is no longer used.  npm stores temporary files in a special
    location in the cache, and they are managed by
    [\`cacache\`](http://npm.im/cacache).
  `,
  description: `
    Historically, the location where temporary files were stored.  No longer
    relevant.
  `,
})

define('umask', {
  default: 0,
  type: Umask,
  description: `
    The "umask" value to use when setting the file creation mode on files and
    folders.

    Folders and executables are given a mode which is \`0o777\` masked
    against this value.  Other files are given a mode which is \`0o666\`
    masked against this value.

    Note that the underlying system will _also_ apply its own umask value to
    files and folders that are created, and npm does not circumvent this, but
    rather adds the \`--umask\` config to it.

    Thus, the effective default umask value on most POSIX systems is 0o22,
    meaning that folders and executables are created with a mode of 0o755 and
    other files are created with a mode of 0o644.
  `,
  flatten,
})

define('unicode', {
  default: unicode,
  defaultDescription: `
    false on windows, true on mac/unix systems with a unicode locale, as
    defined by the \`LC_ALL\`, \`LC_CTYPE\`, or \`LANG\` environment variables.
  `,
  type: Boolean,
  description: `
    When set to true, npm uses unicode characters in the tree output.  When
    false, it uses ascii characters instead of unicode glyphs.
  `,
})

define('update-notifier', {
  default: true,
  type: Boolean,
  description: `
    Set to false to suppress the update notification when using an older
    version of npm than the latest.
  `,
})

define('usage', {
  default: false,
  type: Boolean,
  short: ['?', 'H', 'h'],
  description: `
    Show short usage output about the command specified.
  `,
})

define('user-agent', {
  default: 'npm/{npm-version} ' +
           'node/{node-version} ' +
           '{platform} ' +
           '{arch} ' +
           'workspaces/{workspaces} ' +
           '{ci}',
  type: String,
  description: `
    Sets the User-Agent request header.  The following fields are replaced
    with their actual counterparts:

    * \`{npm-version}\` - The npm version in use
    * \`{node-version}\` - The Node.js version in use
    * \`{platform}\` - The value of \`process.platform\`
    * \`{arch}\` - The value of \`process.arch\`
    * \`{workspaces}\` - Set to \`true\` if the \`workspaces\` or \`workspace\`
      options are set.
    * \`{ci}\` - The value of the \`ci-name\` config, if set, prefixed with
      \`ci/\`, or an empty string if \`ci-name\` is empty.
  `,
  flatten (key, obj, flatOptions) {
    const value = obj[key]
    const ciName = obj['ci-name']
    let inWorkspaces = false
    if (obj.workspaces || obj.workspace && obj.workspace.length) {
      inWorkspaces = true
    }
    flatOptions.userAgent =
      value.replace(/\{node-version\}/gi, obj['node-version'])
        .replace(/\{npm-version\}/gi, obj['npm-version'])
        .replace(/\{platform\}/gi, process.platform)
        .replace(/\{arch\}/gi, process.arch)
        .replace(/\{workspaces\}/gi, inWorkspaces)
        .replace(/\{ci\}/gi, ciName ? `ci/${ciName}` : '')
        .trim()

    // We can't clobber the original or else subsequent flattening will fail
    // (i.e. when we change the underlying config values)
    // obj[key] = flatOptions.userAgent

    // user-agent is a unique kind of config item that gets set from a template
    // and ends up translated.  Because of this, the normal "should we set this
    // to process.env also doesn't work
    process.env.npm_config_user_agent = flatOptions.userAgent
  },
})

define('userconfig', {
  default: '~/.npmrc',
  type: path,
  description: `
    The location of user-level configuration settings.

    This may be overridden by the \`npm_config_userconfig\` environment
    variable or the \`--userconfig\` command line option, but may _not_
    be overridden by settings in the \`globalconfig\` file.
  `,
})

define('version', {
  default: false,
  type: Boolean,
  short: 'v',
  description: `
    If true, output the npm version and exit successfully.

    Only relevant when specified explicitly on the command line.
  `,
})

define('versions', {
  default: false,
  type: Boolean,
  description: `
    If true, output the npm version as well as node's \`process.versions\`
    map and the version in the current working directory's \`package.json\`
    file if one exists, and exit successfully.

    Only relevant when specified explicitly on the command line.
  `,
})

define('viewer', {
  default: isWindows ? 'browser' : 'man',
  defaultDescription: `
    "man" on Posix, "browser" on Windows
  `,
  type: String,
  description: `
    The program to use to view help content.

    Set to \`"browser"\` to view html help content in the default web browser.
  `,
})

define('which', {
  default: null,
  hint: '<fundingSourceNumber>',
  type: [null, Number],
  description: `
    If there are multiple funding sources, which 1-indexed source URL to open.
  `,
})

define('workspace', {
  default: [],
  type: [String, Array],
  hint: '<workspace-name>',
  short: 'w',
  envExport: false,
  description: `
    Enable running a command in the context of the configured workspaces of the
    current project while filtering by running only the workspaces defined by
    this configuration option.

    Valid values for the \`workspace\` config are either:

    * Workspace names
    * Path to a workspace directory
    * Path to a parent workspace directory (will result in selecting all
      workspaces within that folder)

    When set for the \`npm init\` command, this may be set to the folder of
    a workspace which does not yet exist, to create the folder and set it
    up as a brand new workspace within the project.
  `,
  flatten: (key, obj, flatOptions) => {
    definitions['user-agent'].flatten('user-agent', obj, flatOptions)
  },
})

define('workspaces', {
  default: null,
  type: [null, Boolean],
  short: 'ws',
  envExport: false,
  description: `
    Set to true to run the command in the context of **all** configured
    workspaces.

    Explicitly setting this to false will cause commands like \`install\` to
    ignore workspaces altogether.
    When not set explicitly:

    - Commands that operate on the \`node_modules\` tree (install, update,
      etc.) will link workspaces into the \`node_modules\` folder.
    - Commands that do other things (test, exec, publish, etc.) will operate
      on the root project, _unless_ one or more workspaces are specified in
      the \`workspace\` config.
  `,
  flatten: (key, obj, flatOptions) => {
    definitions['user-agent'].flatten('user-agent', obj, flatOptions)

    // TODO: this is a derived value, and should be reworked when we have a
    // pattern for derived value

    // workspacesEnabled is true whether workspaces is null or true
    // commands contextually work with workspaces or not regardless of
    // configuration, so we need an option specifically to disable workspaces
    flatOptions.workspacesEnabled = obj[key] !== false
  },
})

define('workspaces-update', {
  default: true,
  type: Boolean,
  description: `
    If set to true, the npm cli will run an update after operations that may
    possibly change the workspaces installed to the \`node_modules\` folder.
  `,
  flatten,
})

define('yes', {
  default: null,
  type: [null, Boolean],
  short: 'y',
  description: `
    Automatically answer "yes" to any prompts that npm might print on
    the command line.
  `,
})
