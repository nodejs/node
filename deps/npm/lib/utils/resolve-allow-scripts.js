const { log } = require('proc-log')
const npa = require('npm-package-arg')
const pkgJson = require('@npmcli/package-json')
const { isExactVersionDisjunction } = require('@npmcli/arborist/lib/script-allowed.js')
const parseAllowScriptsList = require('@npmcli/config/lib/parse-allow-scripts-list.js')

const buildPolicyFromNames = (names) => {
  /* istanbul ignore if: callers only pass non-empty arrays */
  if (!names.length) {
    return null
  }
  const policy = {}
  for (const name of names) {
    policy[name] = true
  }
  return policy
}

// Read the `allow-scripts` value from one or more named config sources and
// build a policy object. Returns `null` if none of the sources has a value.
const policyFromSources = (npm, sources) => {
  for (const where of sources) {
    const value = npm.config.get?.('allow-scripts', where)
    if (value === undefined) {
      continue
    }
    const names = parseAllowScriptsList(value)
    /* istanbul ignore else: parseAllowScriptsList returns non-empty when value is set */
    if (names.length) {
      return buildPolicyFromNames(names)
    }
  }
  return null
}

const validatePolicy = (policy, sourceLabel) => {
  // Drop and warn about keys with forbidden semver ranges (^, ~, >=, <, *).
  // The RFC only permits exact versions joined by `||`. Bare names like
  // `canvas` and explicit name-only wildcards (`canvas@*`) are allowed.
  if (!policy) {
    return policy
  }
  const cleaned = {}
  for (const [key, value] of Object.entries(policy)) {
    let parsed
    try {
      parsed = npa(key)
    } catch {
      log.warn('install-scripts', `${sourceLabel}: ignoring unparseable entry "${key}"`)
      continue
    }
    if (parsed.type === 'tag') {
      // `pkg@latest`, `pkg@next`, etc. look like a pin but behave name-
      // only — the matcher has no way to verify what the tag points at
      // when scripts run. Reject for the same reason as semver ranges.
      log.warn(
        'install-scripts',
        `${sourceLabel}: ignoring "${key}" — dist-tag specs (@latest, @next, ...) are not allowed; ` +
        'use exact versions joined by "||", or the bare package name, instead'
      )
      continue
    }
    if (parsed.type === 'range') {
      const isNameOnly = parsed.fetchSpec === '*'
        || parsed.rawSpec === ''
        || parsed.rawSpec === '*'
      if (!isNameOnly && !isExactVersionDisjunction(parsed.fetchSpec)) {
        log.warn(
          'install-scripts',
          `${sourceLabel}: ignoring "${key}" — semver ranges (^, ~, >=, <) are not allowed; ` +
          'use exact versions joined by "||" instead'
        )
        continue
      }
    }
    cleaned[key] = value
  }
  return Object.keys(cleaned).length > 0 ? cleaned : null
}

// Resolve the effective allowScripts policy from the layered sources.
// Returns `{ policy, source }` where:
//   - `policy` is an object map of `package-spec` -> boolean, or `null` if
//     no layer has any configuration
//   - `source` is one of `'cli'`, `'package.json'`, `'.npmrc'`, or `null`
//
// Precedence order (highest to lowest), per RFC npm/rfcs#868:
//   1. CLI flags (--allow-scripts) and env vars
//   2. Root `package.json#allowScripts`
//   3. `.npmrc` cascade (project, user, global)
//
// The project `package.json` layer is skipped when:
//   - `npm.global` is true (no project context exists for global installs)
//   - `skipProjectConfig` is true (e.g. npm exec / npx, which per the RFC
//     consult only user/global .npmrc)
//
// In both skipped cases, the CLI and .npmrc layers are still consulted;
// only the project package.json layer is skipped.
//
// The first source with any configuration wins for the entire install;
// lower layers are ignored. A `log.warn` is emitted whenever a setting is
// being suppressed by a higher-priority source.
//
// Reads `package.json` from `npm.prefix` (not `npm.localPrefix`) so an
// install run from a workspace sub-directory still picks up the project
// root's policy.
const resolveAllowScripts = async (npm, { skipProjectConfig = false } = {}) => {
  // Independently probe each RFC layer.
  const cliPolicy = policyFromSources(npm, ['cli', 'env'])
  const npmrcPolicy = policyFromSources(npm, ['project', 'user', 'global', 'builtin'])

  // The --allow-scripts CLI flag is intended for one-off and global
  // contexts (npm exec, npx, npm install -g). In a project-scoped install,
  // team policy belongs in package.json or .npmrc, so reject the flag
  // outright to avoid the "works on my machine" footgun.
  if (cliPolicy && !npm.global && !skipProjectConfig) {
    throw Object.assign(
      new Error(
        '--allow-scripts is not allowed in project-scoped installs. ' +
        'Add the entries to the "allowScripts" field in package.json, ' +
        'or to .npmrc, instead.'
      ),
      { code: 'EALLOWSCRIPTS' }
    )
  }

  // Project package.json is consulted only when the caller is operating
  // inside a real project (not -g, not npx).
  let pkgPolicy = null
  if (!npm.global && !skipProjectConfig) {
    try {
      const { content } = await pkgJson.normalize(npm.prefix)
      if (content?.allowScripts && typeof content.allowScripts === 'object') {
        const entries = Object.entries(content.allowScripts)
        if (entries.length > 0) {
          pkgPolicy = Object.fromEntries(entries)
        }
      }
    } catch (err) {
      log.silly('install-scripts', 'no package.json at prefix', err.message)
    }
  }

  // Validate each candidate layer: drop forbidden ranges, warn the user.
  const cli = validatePolicy(cliPolicy, 'CLI flag')
  const pkg = validatePolicy(pkgPolicy, 'package.json')
  const rc = validatePolicy(npmrcPolicy, '.npmrc')

  // Apply RFC precedence.
  if (cli) {
    // Note: `pkg` is never set alongside `cli` here. Project package.json is
    // only read when `!npm.global && !skipProjectConfig`, but in that same
    // case a CLI policy throws above. With `npm.global` or skipProjectConfig
    // set, package.json is never consulted.
    if (rc) {
      log.warn(
        'install-scripts',
        '.npmrc allow-scripts setting is being ignored because --allow-scripts was passed on the command line'
      )
    }
    return { policy: cli, source: 'cli' }
  }

  if (pkg) {
    if (rc) {
      log.warn(
        'install-scripts',
        '.npmrc allow-scripts setting is being ignored because package.json declares its own allowScripts field'
      )
    }
    return { policy: pkg, source: 'package.json' }
  }

  if (rc) {
    return { policy: rc, source: '.npmrc' }
  }

  return { policy: null, source: null }
}

module.exports = resolveAllowScripts
