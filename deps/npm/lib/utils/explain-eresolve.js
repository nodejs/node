// this is called when an ERESOLVE error is caught in the error-handler,
// or when there's a log.warn('eresolve', msg, explanation), to turn it
// into a human-intelligible explanation of what's wrong and how to fix.
//
// TODO: abstract out the explainNode methods into a separate util for
// use by a future `npm explain <path || spec>` command.

const npm = require('../npm.js')
const { writeFileSync } = require('fs')
const { resolve } = require('path')
const { explainEdge, explainNode, printNode } = require('./explain-dep.js')

// expl is an explanation object that comes from Arborist.  It looks like:
// Depth is how far we want to want to descend into the object making a report.
// The full report (ie, depth=Infinity) is always written to the cache folder
// at ${cache}/eresolve-report.txt along with full json.
const explainEresolve = (expl, color, depth) => {
  const { edge, current, peerConflict } = expl

  const out = []
  if (edge.from && edge.from.whileInstalling)
    out.push('While resolving: ' + printNode(edge.from.whileInstalling, color))

  out.push('Found: ' + explainNode(current, depth, color))
  out.push('\nCould not resolve dependency:\n' +
    explainEdge(edge, depth, color))

  if (peerConflict) {
    const heading = '\nConflicting peer dependency:'
    const pc = explainNode(peerConflict, depth, color)
    out.push(heading + ' ' + pc)
  }

  return out.join('\n')
}

// generate a full verbose report and tell the user how to fix it
const report = (expl, depth = 4) => {
  const fullReport = resolve(npm.cache, 'eresolve-report.txt')

  const orNoStrict = expl.strictPeerDeps ? '--no-strict-peer-deps, ' : ''
  const fix = `Fix the upstream dependency conflict, or retry
this command with ${orNoStrict}--force, or --legacy-peer-deps
to accept an incorrect (and potentially broken) dependency resolution.`

  writeFileSync(fullReport, `# npm resolution error report

${new Date().toISOString()}

${explainEresolve(expl, false, Infinity)}

${fix}

Raw JSON explanation object:

${JSON.stringify(expl, null, 2)}
`, 'utf8')

  return explainEresolve(expl, npm.color, depth) +
    `\n\n${fix}\n\nSee ${fullReport} for a full report.`
}

// the terser explain method for the warning when using --force
const explain = (expl, depth = 2) => explainEresolve(expl, npm.color, depth)

module.exports = {
  explain,
  report,
}
