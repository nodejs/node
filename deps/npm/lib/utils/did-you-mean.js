var meant = require('meant')
var output = require('./output.js')

function didYouMean (scmd, commands) {
  var bestSimilarity = meant(scmd, commands).map(function (str) {
    return '    ' + str
  })

  if (bestSimilarity.length === 0) return
  if (bestSimilarity.length === 1) {
    output('\nDid you mean this?\n' + bestSimilarity[0])
  } else {
    output(
      ['\nDid you mean one of these?']
        .concat(bestSimilarity.slice(0, 3)).join('\n')
    )
  }
}

module.exports = didYouMean
