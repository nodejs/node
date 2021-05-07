const leven = require('leven')
const readJson = require('read-package-json-fast')
const { cmdList } = require('./cmd-list.js')

const didYouMean = async (npm, path, scmd) => {
  const bestCmd = cmdList
    .filter(cmd => leven(scmd, cmd) < scmd.length * 0.4 && scmd !== cmd)
    .map(str => `    npm ${str} # ${npm.commands[str].description}`)

  const pkg = await readJson(`${path}/package.json`)
  const { scripts } = pkg
  // We would already be suggesting this in `npm x` so omit them here
  const runScripts = ['stop', 'start', 'test', 'restart']
  const bestRun = Object.keys(scripts || {})
    .filter(cmd => leven(scmd, cmd) < scmd.length * 0.4 &&
      !runScripts.includes(cmd))
    .map(str => `    npm run ${str} # run the "${str}" package script`)

  const { bin } = pkg
  const bestBin = Object.keys(bin || {})
    .filter(cmd => leven(scmd, cmd) < scmd.length * 0.4)
    .map(str => `    npm exec ${str} # run the "${str}" command from either this or a remote npm package`)

  const best = [...bestCmd, ...bestRun, ...bestBin]

  if (best.length === 0)
    return ''

  const suggestion = best.length === 1 ? `\n\nDid you mean this?\n${best[0]}`
    : `\n\nDid you mean one of these?\n${best.slice(0, 3).join('\n')}`
  return suggestion
}
module.exports = didYouMean
