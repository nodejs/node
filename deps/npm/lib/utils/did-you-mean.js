const leven = require('leven')

const didYouMean = (scmd, commands) => {
  const best = commands
    .filter(cmd => leven(scmd, cmd) < scmd.length * 0.4)
    .map(str => `    ${str}`)
  return best.length === 0 ? ''
    : best.length === 1 ? `\nDid you mean this?\n${best[0]}`
    : `\nDid you mean one of these?\n${best.slice(0, 3).join('\n')}`
}

module.exports = didYouMean
