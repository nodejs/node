const Arborist = require('../')

const printTree = require('./lib/print-tree.js')
const log = require('./lib/logging.js')

const Vuln = require('../lib/vuln.js')
const printReport = report => {
  for (const vuln of report.values()) {
    log.info(printVuln(vuln))
  }
  if (report.topVulns.size) {
    log.info('\n# top-level vulnerabilities')
    for (const vuln of report.topVulns.values()) {
      log.info(printVuln(vuln))
    }
  }
}

const printVuln = vuln => {
  return {
    __proto__: { constructor: Vuln },
    name: vuln.name,
    issues: [...vuln.advisories].map(a => printAdvisory(a)),
    range: vuln.simpleRange,
    nodes: [...vuln.nodes].map(node => `${node.name} ${node.location || '#ROOT'}`),
    ...(vuln.topNodes.size === 0 ? {} : {
      topNodes: [...vuln.topNodes].map(node => `${node.location || '#ROOT'}`),
    }),
  }
}

const printAdvisory = a => `${a.title}${a.url ? ' ' + a.url : ''}`

module.exports = (options, time) => {
  const arb = new Arborist(options)
  return arb
    .audit(options)
    .then(time)
    .then(async ({ timing, result: tree }) => {
      if (options.fix) {
        printTree(tree)
      }
      printReport(arb.auditReport)
      if (tree.meta && options.save) {
        await tree.meta.save()
      }
      return options.fix
        ? `resolved ${tree.inventory.size} deps in ${timing.seconds}`
        : `done in ${timing.seconds}`
    })
}
