// mixin implementing the audit method

const AuditReport = require('../audit-report.js')

// shared with reify
const _global = Symbol.for('global')
const _workspaces = Symbol.for('workspaces')

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
    const tree = await this.loadVirtual()
    if (this[_workspaces] && this[_workspaces].length)
      options.filterSet = this.workspaceDependencySet(tree, this[_workspaces])
    this.auditReport = await AuditReport.load(tree, options)
    const ret = options.fix ? this.reify(options) : this.auditReport
    process.emit('timeEnd', 'audit')
    this.finishTracker('audit')
    return ret
  }
}
