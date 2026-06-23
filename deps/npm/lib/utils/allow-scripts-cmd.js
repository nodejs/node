const { log, output } = require('proc-log')
const npa = require('npm-package-arg')
const semver = require('semver')
const pkgJson = require('@npmcli/package-json')
const { trustedDisplay } = require('@npmcli/arborist/lib/script-allowed.js')
const checkAllowScripts = require('./check-allow-scripts.js')
const resolveAllowScripts = require('./resolve-allow-scripts.js')
const {
  applyApprovalForPackage,
  applyDenyForPackage,
  nameKeyFor,
} = require('./allow-scripts-writer.js')
const BaseCommand = require('../base-cmd.js')

// Parse a positional arg into a name and an optional version range. A bare
// name matches every installed version; `pkg@1.2.3` or `pkg@^1` narrows by
// semver. npm-package-arg handles dotted and scoped names; fall back to the
// raw string as the name if it can't be parsed.
const parsePositional = (arg) => {
  let parsed
  try {
    parsed = npa(arg)
  } catch {
    return { name: arg, range: null }
  }
  const name = parsed.name || arg
  if (parsed.type === 'version' || parsed.type === 'range') {
    const spec = parsed.fetchSpec
    const range = (!spec || spec === '*' || parsed.rawSpec === '' || parsed.rawSpec === '*')
      ? null
      : spec
    return { name, range }
  }
  return { name, range: null }
}

// Shared implementation for `npm approve-scripts` and `npm deny-scripts`.
// Subclasses set `verb` to `'approve'` or `'deny'`.
//
// Extends `BaseCommand` rather than `ArboristCmd` on purpose. Per RFC,
// `allowScripts` is read from the workspace root's `package.json` only;
// individual workspaces don't have their own `allowScripts` field, and
// running approve/deny inside a sub-workspace is identical to running
// it at the root. There's no per-workspace targeting to do, so the
// `--workspace` / `--workspaces` / `--include-workspace-root` params
// from `ArboristCmd` would be misleading no-ops.
class AllowScriptsCmd extends BaseCommand {
  static params = ['all', 'allow-scripts-pending', 'allow-scripts-pin', 'json']
  static ignoreImplicitWorkspace = false

  // Subclasses set `static verb = 'approve' | 'deny'`.
  get verb () {
    /* istanbul ignore next: every concrete subclass declares static verb */
    return this.constructor.verb
  }

  async exec (args) {
    if (this.npm.global) {
      throw Object.assign(
        new Error(`\`npm ${this.constructor.name}\` does not work for global installs`),
        { code: 'EGLOBAL' }
      )
    }

    const pending = !!this.npm.config.get('allow-scripts-pending')
    const all = !!this.npm.config.get('all')

    if (pending && (args.length > 0 || all)) {
      throw this.usageError(
        '`--allow-scripts-pending` cannot be combined with positional arguments or `--all`.'
      )
    }
    if (!pending && !all && args.length === 0) {
      throw this.usageError()
    }
    if (this.verb === 'deny' && pending) {
      throw this.usageError('`npm deny-scripts --allow-scripts-pending` is not supported.')
    }

    const Arborist = require('@npmcli/arborist')
    const { policy } = await resolveAllowScripts(this.npm)
    const arb = new Arborist({
      ...this.npm.flatOptions,
      path: this.npm.prefix,
      allowScripts: policy,
    })
    await arb.loadActual()

    // Keep listing unreviewed packages even with ignore-scripts set, so
    // you can move from a blanket ignore-scripts to an allowlist. This
    // only lists; nothing runs.
    const unreviewed = await checkAllowScripts({ arb, npm: this.npm, includeWhenIgnored: true })

    if (pending) {
      return this.runPending(unreviewed)
    }

    if (all) {
      return this.runAll(unreviewed)
    }

    return this.runPositional(args, arb)
  }

  runPending (unreviewed) {
    if (this.npm.flatOptions.json) {
      output.buffer({ allowScripts: this.pendingSummary(unreviewed) })
      return
    }
    if (unreviewed.length === 0) {
      output.standard('No packages with unreviewed install scripts.')
      return
    }
    const count = unreviewed.length
    const has = count === 1 ? 'has' : 'have'
    const pkg = count === 1 ? 'package' : 'packages'
    output.standard(
      `${count} ${pkg} ${has} install scripts blocked because they are not covered by allowScripts:`
    )
    for (const { node, scripts } of unreviewed) {
      const { name, version } = trustedDisplay(node)
      /* istanbul ignore next: every test node has a name */
      const display = name || '<unknown>'
      const ver = version ? `@${version}` : ''
      const events = Object.entries(scripts)
        .map(([event, cmd]) => `${event}: ${cmd}`)
        .join('; ')
      output.standard(`  ${display}${ver} (${events})`)
    }
    output.standard('')
    output.standard(
      'Run `npm approve-scripts <pkg>` to allow, or `npm deny-scripts <pkg>` to deny.'
    )
  }

  // Build the same `{ name, changes }` shape printSummary uses for writes,
  // but tag every entry as `pending` since nothing is written. Names and
  // versions are derived exactly like the text listing above.
  pendingSummary (unreviewed) {
    const groups = new Map()
    for (const { node } of unreviewed) {
      const { name, version } = trustedDisplay(node)
      /* istanbul ignore next: every test node has a name */
      const display = name || '<unknown>'
      const key = version ? `${display}@${version}` : display
      if (!groups.has(display)) {
        groups.set(display, [])
      }
      groups.get(display).push({ key, change: 'pending' })
    }
    return [...groups].map(([name, changes]) => ({ name, changes }))
  }

  async runAll (unreviewed) {
    if (unreviewed.length === 0) {
      if (this.npm.flatOptions.json) {
        output.buffer({ allowScripts: [] })
        return
      }
      output.standard('No packages with unreviewed install scripts.')
      return
    }
    // Bundled dependencies never appear in `unreviewed` (checkAllowScripts
    // skips them because they never run their install scripts and cannot
    // be allowlisted), so there is nothing extra to filter here.
    const groups = this.groupByPackage(unreviewed.map(({ node }) => node))
    await this.writePolicyChanges(groups)
  }

  async runPositional (args, arb) {
    const { matched, unmatched } = this.findNodesForArgs(args, arb)
    if (unmatched.length > 0) {
      throw Object.assign(
        new Error(`No installed packages match: ${unmatched.join(', ')}`),
        { code: 'ENOMATCH' }
      )
    }
    const groups = this.groupByPackage(matched)
    /* istanbul ignore if: matched is non-empty here; groups only empties when a
       matched node has no trusted key, which groupByPackage already warns on */
    if (Object.keys(groups).length === 0) {
      throw Object.assign(
        new Error(`No installed packages match: ${args.join(', ')}`),
        { code: 'ENOMATCH' }
      )
    }
    await this.writePolicyChanges(groups)
  }

  findNodesForArgs (args, arb) {
    // Match positional args against each node's trusted name. Registry deps
    // use the URL-derived name; non-registry deps fall back to the dependency
    // edge name. A version or range on the arg narrows the match to installed
    // versions that satisfy it. Bundled deps are excluded for the same reason
    // as --all. Args that match nothing are returned in `unmatched`.
    const matched = []
    const unmatched = []
    for (const arg of args) {
      const { name: wantName, range } = parsePositional(arg)
      const found = []
      for (const node of arb.actualTree.inventory.values()) {
        if (node.isProjectRoot || node.isWorkspace || node.inBundle) {
          continue
        }
        const { name, version } = trustedDisplay(node)
        if (!name || name !== wantName) {
          continue
        }
        if (range && (!version || !semver.satisfies(version, range, { loose: true }))) {
          continue
        }
        found.push(node)
      }
      if (found.length === 0) {
        unmatched.push(arg)
      } else {
        matched.push(...found)
      }
    }
    return { matched, unmatched }
  }

  get logTitle () {
    return this.constructor.name.replace(/([a-z])([A-Z])/g, '$1-$2').toLowerCase()
  }

  groupByPackage (nodes) {
    const groups = {}
    for (const node of nodes) {
      const key = nameKeyFor(node)
      /* istanbul ignore if: callers prefilter via inBundle and trustedDisplay so untrusted nodes don't reach here */
      if (!key) {
        log.warn(
          this.logTitle,
          `skipping ${node.name || '<unknown>'}: no trusted identity for policy key`
        )
        continue
      }
      if (!groups[key]) {
        groups[key] = []
      }
      groups[key].push(node)
    }
    return groups
  }

  async writePolicyChanges (groups) {
    const pin = this.npm.config.get('allow-scripts-pin') !== false

    const pkg = await pkgJson.load(this.npm.prefix)
    const content = pkg.content
    const existing = content.allowScripts && typeof content.allowScripts === 'object'
      ? content.allowScripts
      : {}

    let updated = existing
    const summary = []

    for (const [name, nodes] of Object.entries(groups)) {
      const result = this.verb === 'approve'
        ? applyApprovalForPackage(updated, nodes, { pin })
        : applyDenyForPackage(updated, nodes)

      if (result.warning) {
        log.warn(this.logTitle, result.warning)
      }
      updated = result.allowScripts
      summary.push({ name, changes: result.changes })
    }

    /* istanbul ignore else: writePolicyChanges only called when changes are expected */
    if (updated !== existing) {
      pkg.update({ allowScripts: updated })
      await pkg.save()
    }

    this.printSummary(summary)
  }

  printSummary (summary) {
    if (this.npm.flatOptions.json) {
      output.buffer({ allowScripts: summary })
      return
    }
    const verb = this.verb === 'approve' ? 'Approved' : 'Denied'
    let touched = 0
    for (const { name, changes } of summary) {
      if (changes.length === 0) {
        continue
      }
      touched++
      output.standard(`${verb} ${name}:`)
      for (const { key, change } of changes) {
        output.standard(`  ${change} ${key}`)
      }
    }
    if (touched === 0) {
      output.standard(`Nothing to ${this.verb}; allowScripts unchanged.`)
    }
  }
}

module.exports = AllowScriptsCmd
