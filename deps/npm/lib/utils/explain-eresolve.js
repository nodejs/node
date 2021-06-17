// this is called when an ERESOLVE error is caught in the error-handler,
// or when there's a log.warn('eresolve', msg, explanation), to turn it
// into a human-intelligible explanation of what's wrong and how to fix.
const { writeFileSync } = require('fs')
const { explainEdge, explainNode, printNode } = require('./explain-dep.js')

// expl is an explanation object that comes from Arborist.  It looks like:
// Depth is how far we want to want to descend into the object making a report.
// The full report (ie, depth=Infinity) is always written to the cache folder
// at ${cache}/eresolve-report.txt along with full json.
const explain = (expl, color, depth) => {
  const { edge, current, peerConflict, currentEdge } = expl

  const out = []
  if (edge.from && edge.from.whileInstalling)
    out.push('While resolving: ' + printNode(edge.from.whileInstalling, color))

  // it "should" be impossible for an ERESOLVE explanation to lack both
  // current and currentEdge, but better to have a less helpful error
  // than a crashing failure.
  if (current)
    out.push('Found: ' + explainNode(current, depth, color))
  else if (currentEdge)
    out.push('Found: ' + explainEdge(currentEdge, depth, color))

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
const report = (expl, color, fullReport) => {
  const orNoStrict = expl.strictPeerDeps ? '--no-strict-peer-deps, ' : ''
  const fix = `Fix the upstream dependency conflict, or retry
this command with ${orNoStrict}--force, or --legacy-peer-deps
to accept an incorrect (and potentially broken) dependency resolution.`

  writeFileSync(fullReport, `# npm resolution error report

${new Date().toISOString()}

${explain(expl, false, Infinity)}

${fix}

Raw JSON explanation object:

${JSON.stringify(expl, null, 2)}
`, 'utf8')

  return explain(expl, color, 4) +
    `\n\n${fix}\n\nSee ${fullReport} for a full report.`
}

module.exports = {
  explain,
  report,
}
