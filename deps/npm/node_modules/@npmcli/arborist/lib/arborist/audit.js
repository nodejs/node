// mixin implementing the audit method

const AuditReport = require('../audit-report.js')

// shared with reify
const _global = Symbol.for('global')
const _workspaces = Symbol.for('workspaces')
const _includeWorkspaceRoot = Symbol.for('includeWorkspaceRoot')

module.exports = cls => class Auditor extends cls {
  async audit (options = {}) {
    this.addTracker('audit')
    if (this[_global]) {
      throw Object.assign(
        new Error('`npm audit` does not support testing globals'),
        { code: 'EAUDITGLOBAL' }
      )
    }

    // allow the user to set options on the ctor as well.
    // XXX: deprecate separate method options objects.
    options = { ...this.options, ...options }

    process.emit('time', 'audit')
    let tree
    if (options.packageLock === false) {
      // build ideal tree
      await this.loadActual(options)
      await this.buildIdealTree()
      tree = this.idealTree
    } else {
      tree = await this.loadVirtual()
    }
    if (this[_workspaces] && this[_workspaces].length) {
      options.filterSet = this.workspaceDependencySet(
        tree,
        this[_workspaces],
        this[_includeWorkspaceRoot]
      )
    }
    if (!options.workspacesEnabled) {
      options.filterSet =
        this.excludeWorkspacesDependencySet(tree)
    }
    this.auditReport = await AuditReport.load(tree, options)
    const ret = options.fix ? this.reify(options) : this.auditReport
    process.emit('timeEnd', 'audit')
    this.finishTracker('audit')
    return ret
  }
}
