// this is called when an ERESOLVE error is caught in the exit-handler,
// or when there's a log.warn('eresolve', msg, explanation), to turn it
// into a human-intelligible explanation of what's wrong and how to fix.
const { explainEdge, explainNode, printNode } = require('./explain-dep.js')

// expl is an explanation object that comes from Arborist.  It looks like:
// Depth is how far we want to want to descend into the object making a report.
// The full report (ie, depth=Infinity) is always written to the cache folder
// at ${cache}/eresolve-report.txt along with full json.
const explain = (expl, chalk, depth) => {
  const { edge, dep, current, peerConflict, currentEdge } = expl

  const out = []
  const whileInstalling = dep && dep.whileInstalling ||
    current && current.whileInstalling ||
    edge && edge.from && edge.from.whileInstalling
  if (whileInstalling) {
    out.push('While resolving: ' + printNode(whileInstalling, chalk))
  }

  // it "should" be impossible for an ERESOLVE explanation to lack both
  // current and currentEdge, but better to have a less helpful error
  // than a crashing failure.
  if (current) {
    out.push('Found: ' + explainNode(current, depth, chalk))
  } else if (peerConflict && peerConflict.current) {
    out.push('Found: ' + explainNode(peerConflict.current, depth, chalk))
  } else if (currentEdge) {
    out.push('Found: ' + explainEdge(currentEdge, depth, chalk))
  } else /* istanbul ignore else - should always have one */ if (edge) {
    out.push('Found: ' + explainEdge(edge, depth, chalk))
  }

  out.push('\nCould not resolve dependency:\n' +
    explainEdge(edge, depth, chalk))

  if (peerConflict) {
    const heading = '\nConflicting peer dependency:'
    const pc = explainNode(peerConflict.peer, depth, chalk)
    out.push(heading + ' ' + pc)
  }

  return out.join('\n')
}

// generate a full verbose report and tell the user how to fix it
const report = (expl, chalk, noColor) => {
  const flags = [
    expl.strictPeerDeps ? '--no-strict-peer-deps' : '',
    '--force',
    '--legacy-peer-deps',
  ].filter(Boolean)

  const or = (arr) => arr.length <= 2
    ? arr.join(' or ') :
    arr.map((v, i, l) => i + 1 === l.length ? `or ${v}` : v).join(', ')

  const fix = `Fix the upstream dependency conflict, or retry
this command with ${or(flags)}
to accept an incorrect (and potentially broken) dependency resolution.`

  return {
    explanation: `${explain(expl, chalk, 4)}\n\n${fix}`,
    file: `# npm resolution error report\n\n${explain(expl, noColor, Infinity)}\n\n${fix}`,
  }
}

module.exports = {
  explain,
  report,
}
