'use strict'

var gitHosts = module.exports = {
  github: {
    // First two are insecure and generally shouldn't be used any more, but
    // they are still supported.
    'protocols': [ 'git', 'http', 'git+ssh', 'git+https', 'ssh', 'https' ],
    'domain': 'github.com',
    'treepath': 'tree',
    'filetemplate': 'https://{auth@}raw.githubusercontent.com/{user}/{project}/{comittish}/{path}',
    'bugstemplate': 'https://{domain}/{user}/{project}/issues',
    'gittemplate': 'git://{auth@}{domain}/{user}/{project}.git{#comittish}'
  },
  bitbucket: {
    'protocols': [ 'git+ssh', 'git+https', 'ssh', 'https' ],
    'domain': 'bitbucket.org',
    'treepath': 'src'
  },
  gitlab: {
    'protocols': [ 'git+ssh', 'git+https', 'ssh', 'https' ],
    'domain': 'gitlab.com',
    'treepath': 'tree',
    'docstemplate': 'https://{domain}/{user}/{project}{/tree/comittish}#README',
    'bugstemplate': 'https://{domain}/{user}/{project}/issues'
  },
  gist: {
    'protocols': [ 'git', 'git+ssh', 'git+https', 'ssh', 'https' ],
    'domain': 'gist.github.com',
    'pathmatch': /^[/](?:([^/]+)[/])?([a-z0-9]+)(?:[.]git)?$/,
    'filetemplate': 'https://gist.githubusercontent.com/{user}/{project}/raw{/comittish}/{path}',
    'bugstemplate': 'https://{domain}/{project}',
    'gittemplate': 'git://{domain}/{project}.git{#comittish}',
    'sshtemplate': 'git@{domain}:/{project}.git{#comittish}',
    'sshurltemplate': 'git+ssh://git@{domain}/{project}.git{#comittish}',
    'browsetemplate': 'https://{domain}/{project}{/comittish}',
    'docstemplate': 'https://{domain}/{project}{/comittish}',
    'httpstemplate': 'git+https://{domain}/{project}.git{#comittish}',
    'shortcuttemplate': '{type}:{project}{#comittish}',
    'pathtemplate': '{project}{#comittish}'
  }
}

var gitHostDefaults = {
  'sshtemplate': 'git@{domain}:{user}/{project}.git{#comittish}',
  'sshurltemplate': 'git+ssh://git@{domain}/{user}/{project}.git{#comittish}',
  'browsetemplate': 'https://{domain}/{user}/{project}{/tree/comittish}',
  'docstemplate': 'https://{domain}/{user}/{project}{/tree/comittish}#readme',
  'httpstemplate': 'git+https://{auth@}{domain}/{user}/{project}.git{#comittish}',
  'filetemplate': 'https://{domain}/{user}/{project}/raw/{comittish}/{path}',
  'shortcuttemplate': '{type}:{user}/{project}{#comittish}',
  'pathtemplate': '{user}/{project}{#comittish}',
  'pathmatch': /^[/]([^/]+)[/]([^/]+?)(?:[.]git)?$/
}

Object.keys(gitHosts).forEach(function (name) {
  Object.keys(gitHostDefaults).forEach(function (key) {
    if (gitHosts[name][key]) return
    gitHosts[name][key] = gitHostDefaults[key]
  })
  gitHosts[name].protocols_re = RegExp('^(' +
    gitHosts[name].protocols.map(function (protocol) {
      return protocol.replace(/([\\+*{}()\[\]$^|])/g, '\\$1')
    }).join('|') + '):$')
})
