const t = require('tap')
const hostedFromMani = require('../../../lib/utils/hosted-git-info-from-manifest.js')
const hostedGitInfo = require('hosted-git-info')

t.equal(hostedFromMani({}), null)
t.equal(hostedFromMani({ repository: { no: 'url' } }), null)
t.equal(hostedFromMani({ repository: 123 }), null)
t.equal(hostedFromMani({ repository: 'not hosted anywhere' }), null)
t.equal(hostedFromMani({ repository: { url: 'not hosted anywhere' } }), null)

t.match(hostedFromMani({
  repository: 'git+https://github.com/isaacs/abbrev-js',
}), hostedGitInfo.fromUrl('git+https://github.com/isaacs/abbrev-js'))

t.match(hostedFromMani({
  repository: { url: 'git+https://github.com/isaacs/abbrev-js' },
}), hostedGitInfo.fromUrl('https://github.com/isaacs/abbrev-js'))

t.match(hostedFromMani({
  repository: { url: 'git+ssh://git@github.com/isaacs/abbrev-js' },
}), hostedGitInfo.fromUrl('ssh://git@github.com/isaacs/abbrev-js'))
