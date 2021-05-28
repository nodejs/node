'use strict'
const maybeJoin = (...args) => args.every(arg => arg) ? args.join('') : ''
const maybeEncode = (arg) => arg ? encodeURIComponent(arg) : ''

const defaults = {
  sshtemplate: ({ domain, user, project, committish }) => `git@${domain}:${user}/${project}.git${maybeJoin('#', committish)}`,
  sshurltemplate: ({ domain, user, project, committish }) => `git+ssh://git@${domain}/${user}/${project}.git${maybeJoin('#', committish)}`,
  browsetemplate: ({ domain, user, project, committish, treepath }) => `https://${domain}/${user}/${project}${maybeJoin('/', treepath, '/', maybeEncode(committish))}`,
  browsefiletemplate: ({ domain, user, project, committish, treepath, path, fragment, hashformat }) => `https://${domain}/${user}/${project}/${treepath}/${maybeEncode(committish || 'master')}/${path}${maybeJoin('#', hashformat(fragment || ''))}`,
  docstemplate: ({ domain, user, project, treepath, committish }) => `https://${domain}/${user}/${project}${maybeJoin('/', treepath, '/', maybeEncode(committish))}#readme`,
  httpstemplate: ({ auth, domain, user, project, committish }) => `git+https://${maybeJoin(auth, '@')}${domain}/${user}/${project}.git${maybeJoin('#', committish)}`,
  filetemplate: ({ domain, user, project, committish, path }) => `https://${domain}/${user}/${project}/raw/${maybeEncode(committish) || 'master'}/${path}`,
  shortcuttemplate: ({ type, user, project, committish }) => `${type}:${user}/${project}${maybeJoin('#', committish)}`,
  pathtemplate: ({ user, project, committish }) => `${user}/${project}${maybeJoin('#', committish)}`,
  bugstemplate: ({ domain, user, project }) => `https://${domain}/${user}/${project}/issues`,
  hashformat: formatHashFragment
}

const gitHosts = {}
gitHosts.github = Object.assign({}, defaults, {
  // First two are insecure and generally shouldn't be used any more, but
  // they are still supported.
  protocols: ['git:', 'http:', 'git+ssh:', 'git+https:', 'ssh:', 'https:'],
  domain: 'github.com',
  treepath: 'tree',
  filetemplate: ({ auth, user, project, committish, path }) => `https://${maybeJoin(auth, '@')}raw.githubusercontent.com/${user}/${project}/${maybeEncode(committish) || 'master'}/${path}`,
  gittemplate: ({ auth, domain, user, project, committish }) => `git://${maybeJoin(auth, '@')}${domain}/${user}/${project}.git${maybeJoin('#', committish)}`,
  tarballtemplate: ({ domain, user, project, committish }) => `https://codeload.${domain}/${user}/${project}/tar.gz/${maybeEncode(committish) || 'master'}`,
  extract: (url) => {
    let [, user, project, type, committish] = url.pathname.split('/', 5)
    if (type && type !== 'tree') {
      return
    }

    if (!type) {
      committish = url.hash.slice(1)
    }

    if (project && project.endsWith('.git')) {
      project = project.slice(0, -4)
    }

    if (!user || !project) {
      return
    }

    return { user, project, committish }
  }
})

gitHosts.bitbucket = Object.assign({}, defaults, {
  protocols: ['git+ssh:', 'git+https:', 'ssh:', 'https:'],
  domain: 'bitbucket.org',
  treepath: 'src',
  tarballtemplate: ({ domain, user, project, committish }) => `https://${domain}/${user}/${project}/get/${maybeEncode(committish) || 'master'}.tar.gz`,
  extract: (url) => {
    let [, user, project, aux] = url.pathname.split('/', 4)
    if (['get'].includes(aux)) {
      return
    }

    if (project && project.endsWith('.git')) {
      project = project.slice(0, -4)
    }

    if (!user || !project) {
      return
    }

    return { user, project, committish: url.hash.slice(1) }
  }
})

gitHosts.gitlab = Object.assign({}, defaults, {
  protocols: ['git+ssh:', 'git+https:', 'ssh:', 'https:'],
  domain: 'gitlab.com',
  treepath: 'tree',
  httpstemplate: ({ auth, domain, user, project, committish }) => `git+https://${maybeJoin(auth, '@')}${domain}/${user}/${project}.git${maybeJoin('#', committish)}`,
  tarballtemplate: ({ domain, user, project, committish }) => `https://${domain}/${user}/${project}/repository/archive.tar.gz?ref=${maybeEncode(committish) || 'master'}`,
  extract: (url) => {
    const path = url.pathname.slice(1)
    if (path.includes('/-/') || path.includes('/archive.tar.gz')) {
      return
    }

    const segments = path.split('/')
    let project = segments.pop()
    if (project.endsWith('.git')) {
      project = project.slice(0, -4)
    }

    const user = segments.join('/')
    if (!user || !project) {
      return
    }

    return { user, project, committish: url.hash.slice(1) }
  }
})

gitHosts.gist = Object.assign({}, defaults, {
  protocols: ['git:', 'git+ssh:', 'git+https:', 'ssh:', 'https:'],
  domain: 'gist.github.com',
  sshtemplate: ({ domain, project, committish }) => `git@${domain}:${project}.git${maybeJoin('#', committish)}`,
  sshurltemplate: ({ domain, project, committish }) => `git+ssh://git@${domain}/${project}.git${maybeJoin('#', committish)}`,
  browsetemplate: ({ domain, project, committish }) => `https://${domain}/${project}${maybeJoin('/', maybeEncode(committish))}`,
  browsefiletemplate: ({ domain, project, committish, path, hashformat }) => `https://${domain}/${project}${maybeJoin('/', maybeEncode(committish))}${maybeJoin('#', hashformat(path))}`,
  docstemplate: ({ domain, project, committish }) => `https://${domain}/${project}${maybeJoin('/', maybeEncode(committish))}`,
  httpstemplate: ({ domain, project, committish }) => `git+https://${domain}/${project}.git${maybeJoin('#', committish)}`,
  filetemplate: ({ user, project, committish, path }) => `https://gist.githubusercontent.com/${user}/${project}/raw${maybeJoin('/', maybeEncode(committish))}/${path}`,
  shortcuttemplate: ({ type, project, committish }) => `${type}:${project}${maybeJoin('#', committish)}`,
  pathtemplate: ({ project, committish }) => `${project}${maybeJoin('#', committish)}`,
  bugstemplate: ({ domain, project }) => `https://${domain}/${project}`,
  gittemplate: ({ domain, project, committish }) => `git://${domain}/${project}.git${maybeJoin('#', committish)}`,
  tarballtemplate: ({ project, committish }) => `https://codeload.github.com/gist/${project}/tar.gz/${maybeEncode(committish) || 'master'}`,
  extract: (url) => {
    let [, user, project, aux] = url.pathname.split('/', 4)
    if (aux === 'raw') {
      return
    }

    if (!project) {
      if (!user) {
        return
      }

      project = user
      user = null
    }

    if (project.endsWith('.git')) {
      project = project.slice(0, -4)
    }

    return { user, project, committish: url.hash.slice(1) }
  },
  hashformat: function (fragment) {
    return fragment && 'file-' + formatHashFragment(fragment)
  }
})

const names = Object.keys(gitHosts)
gitHosts.byShortcut = {}
gitHosts.byDomain = {}
for (const name of names) {
  gitHosts.byShortcut[`${name}:`] = name
  gitHosts.byDomain[gitHosts[name].domain] = name
}

function formatHashFragment (fragment) {
  return fragment.toLowerCase().replace(/^\W+|\/|\W+$/g, '').replace(/\W+/g, '-')
}

module.exports = gitHosts
