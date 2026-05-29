const { log, output } = require('proc-log')
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

    const unreviewed = await checkAllowScripts({ arb, npm: this.npm })

    if (pending) {
      return this.runPending(unreviewed)
    }

    if (all) {
      return this.runAll(unreviewed)
    }

    return this.runPositional(args, arb)
  }

  runPending (unreviewed) {
    if (unreviewed.length === 0) {
      output.standard('No packages with unreviewed install scripts.')
      return
    }
    const count = unreviewed.length
    const has = count === 1 ? 'has' : 'have'
    const pkg = count === 1 ? 'package' : 'packages'
    output.standard(
      `${count} ${pkg} ${has} install scripts not yet covered by allowScripts:`
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

  async runAll (unreviewed) {
    if (unreviewed.length === 0) {
      output.standard('No packages with unreviewed install scripts.')
      return
    }
    // Bundled dependencies cannot be allowlisted in Phase 1 (RFC defers
    // this to a follow-up because matching by name@version from the
    // bundled tarball would reintroduce manifest confusion). Exclude
    // them from `--all` so we don't silently write a policy entry under
    // attacker-controlled identity.
    const candidates = unreviewed.filter(({ node }) => !node.inBundle)
    const skipped = unreviewed.length - candidates.length
    if (skipped > 0) {
      /* istanbul ignore next: plural variant covered separately */
      const noun = skipped === 1 ? 'dependency' : 'dependencies'
      log.warn(
        this.logTitle,
        `Skipping ${skipped} bundled ${noun}; bundled deps with install ` +
        'scripts cannot be allowlisted in this release.'
      )
    }
    if (candidates.length === 0) {
      output.standard('No packages eligible for approval.')
      return
    }
    const groups = this.groupByPackage(candidates.map(({ node }) => node))
    await this.writePolicyChanges(groups)
  }

  async runPositional (args, arb) {
    const matched = this.findNodesForArgs(args, arb)
    const groups = this.groupByPackage(matched)
    if (Object.keys(groups).length === 0) {
      throw Object.assign(
        new Error(`No installed packages match: ${args.join(', ')}`),
        { code: 'ENOMATCH' }
      )
    }
    await this.writePolicyChanges(groups)
  }

  findNodesForArgs (args, arb) {
    // Match positional args against each node's trusted name. Registry
    // deps use the URL-derived name; non-registry deps fall back to the
    // dependency edge name. Bundled deps are excluded for the same reason
    // as --all.
    const wanted = new Set(args)
    const matched = []
    for (const node of arb.actualTree.inventory.values()) {
      if (node.isProjectRoot || node.isWorkspace || node.inBundle) {
        continue
      }
      const { name } = trustedDisplay(node)
      if (name && wanted.has(name)) {
        matched.push(node)
      }
    }
    return matched
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
