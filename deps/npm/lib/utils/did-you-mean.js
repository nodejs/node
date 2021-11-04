const { distance } = require('fastest-levenshtein')
const readJson = require('read-package-json-fast')
const { cmdList } = require('./cmd-list.js')

const didYouMean = async (npm, path, scmd) => {
  // const cmd = await npm.cmd(str)
  const close = cmdList
    .filter(cmd => distance(scmd, cmd) < scmd.length * 0.4 && scmd !== cmd)
  let best = []
  for (const str of close) {
    const cmd = await npm.cmd(str)
    best.push(`    npm ${str} # ${cmd.description}`)
  }
  // We would already be suggesting this in `npm x` so omit them here
  const runScripts = ['stop', 'start', 'test', 'restart']
  try {
    const { bin, scripts } = await readJson(`${path}/package.json`)
    best = best.concat(
      Object.keys(scripts || {})
        .filter(cmd => distance(scmd, cmd) < scmd.length * 0.4 &&
          !runScripts.includes(cmd))
        .map(str => `    npm run ${str} # run the "${str}" package script`),
      Object.keys(bin || {})
        .filter(cmd => distance(scmd, cmd) < scmd.length * 0.4)
        .map(str => `    npm exec ${str} # run the "${str}" command from either this or a remote npm package`)
    )
  } catch (_) {
    // gracefully ignore not being in a folder w/ a package.json
  }

  if (best.length === 0)
    return ''

  const suggestion = best.length === 1 ? `\n\nDid you mean this?\n${best[0]}`
    : `\n\nDid you mean one of these?\n${best.slice(0, 3).join('\n')}`
  return suggestion
}
module.exports = didYouMean
