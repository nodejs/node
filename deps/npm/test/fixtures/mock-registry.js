/*
 * Mock registry class
 *
 * This should end up as the centralized place where we generate test fixtures
 * for tests against any registry data.
 */
const pacote = require('pacote')
const npa = require('npm-package-arg')
class MockRegistry {
  #tap
  #nock
  #registry
  #authorization
  #basic

  constructor (opts) {
    if (!opts.registry) {
      throw new Error('mock registry requires a registry value')
    }
    this.#registry = (new URL(opts.registry)).origin
    this.#authorization = opts.authorization
    this.#basic = opts.basic
    // Required for this.package
    this.#tap = opts.tap
  }

  get nock () {
    if (!this.#nock) {
      if (!this.#tap) {
        throw new Error('cannot mock packages without a tap fixture')
      }
      const tnock = require('./tnock.js')
      const reqheaders = {}
      if (this.#authorization) {
        reqheaders.authorization = `Bearer ${this.#authorization}`
      }
      if (this.#basic) {
        reqheaders.authorization = `Basic ${this.#basic}`
      }
      this.#nock = tnock(this.#tap, this.#registry, { reqheaders })
    }
    return this.#nock
  }

  set nock (nock) {
    this.#nock = nock
  }

  search ({ responseCode = 200, results = [], error }) {
    // the flags, score, and searchScore parts of the response are never used
    // by npm, only package is used
    const response = results.map(p => ({ package: p }))
    this.nock = this.nock.get('/-/v1/search').query(true)
    if (error) {
      this.nock = this.nock.replyWithError(error)
    } else {
      this.nock = this.nock.reply(responseCode, { objects: response })
    }
    return this.nock
  }

  whoami ({ username, body, responseCode = 200, times = 1 }) {
    if (username) {
      this.nock = this.nock.get('/-/whoami').times(times).reply(responseCode, { username })
    } else {
      this.nock = this.nock.get('/-/whoami').times(times).reply(responseCode, body)
    }
  }

  access ({ spec, access, publishRequires2fa }) {
    const body = {}
    if (access !== undefined) {
      body.access = access
    }
    if (publishRequires2fa !== undefined) {
      body.publish_requires_tfa = publishRequires2fa
    }
    this.nock = this.nock.post(
      `/-/package/${encodeURIComponent(spec)}/access`,
      body
    ).reply(200)
  }

  grant ({ spec, team, permissions }) {
    if (team.startsWith('@')) {
      team = team.slice(1)
    }
    const [scope, teamName] = team.split(':')
    this.nock = this.nock.put(
      `/-/team/${encodeURIComponent(scope)}/${encodeURIComponent(teamName)}/package`,
      { package: spec, permissions }
    ).reply(200)
  }

  revoke ({ spec, team }) {
    if (team.startsWith('@')) {
      team = team.slice(1)
    }
    const [scope, teamName] = team.split(':')
    this.nock = this.nock.delete(
      `/-/team/${encodeURIComponent(scope)}/${encodeURIComponent(teamName)}/package`,
      { package: spec }
    ).reply(200)
  }

  couchuser ({ username, body, responseCode = 200 }) {
    if (body) {
      this.nock = this.nock.get(`/-/user/org.couchdb.user:${encodeURIComponent(username)}`)
        .reply(responseCode, body)
    } else {
      this.nock = this.nock.get(`/-/user/org.couchdb.user:${encodeURIComponent(username)}`)
        .reply(responseCode, { _id: `org.couchdb.user:${username}`, email: '', name: username })
    }
  }

  couchlogin ({ username, password, email, otp, token = 'npm_default-test-token' }) {
    this.nock = this.nock
      .post('/-/v1/login').reply(401, { error: 'You must be logged in to publish packages.' })
    if (otp) {
      // TODO otp failure results in a 401 with
      // {"ok":false,"error":"failed to authenticate: Could not authenticate ${username}: bad otp"}
    }
    this.nock = this.nock.put(`/-/user/org.couchdb.user:${username}`, body => {
      this.#tap.match(body, {
        _id: `org.couchdb.user:${username}`,
        name: username,
        password,
        type: 'user',
        roles: [],
      })
      if (!body.date) {
        return false
      }
      return true
    }).reply(201, {
      ok: true,
      id: 'org.couchdb.user:undefined',
      rev: '_we_dont_use_revs_any_more',
      token,
    })
  }

  // team can be a team or a username
  lsPackages ({ team, packages = {}, times = 1 }) {
    if (team.startsWith('@')) {
      team = team.slice(1)
    }
    const [scope, teamName] = team.split(':')
    let uri
    if (teamName) {
      uri = `/-/team/${encodeURIComponent(scope)}/${encodeURIComponent(teamName)}/package`
    } else {
      uri = `/-/org/${encodeURIComponent(scope)}/package`
    }
    this.nock = this.nock.get(uri).query({ format: 'cli' }).times(times).reply(200, packages)
  }

  lsCollaborators ({ spec, user, collaborators = {} }) {
    const query = { format: 'cli' }
    if (user) {
      query.user = user
    }
    this.nock = this.nock.get(`/-/package/${encodeURIComponent(spec)}/collaborators`)
      .query(query)
      .reply(200, collaborators)
  }

  advisory (advisory = {}) {
    const id = advisory.id || parseInt(Math.random() * 1000000)
    return {
      id,
      url: `https://github.com/advisories/GHSA-${id}`,
      title: `Test advisory ${id}`,
      severity: 'high',
      vulnerable_versions: '*',
      cwe: [
        'cwe-0',
      ],
      cvss: {
        score: 0,
      },
      ...advisory,
    }
  }

  star (manifest, users) {
    const spec = npa(manifest.name)
    this.nock = this.nock.put(`/${spec.escapedName}`, {
      _id: manifest._id,
      _rev: manifest._rev,
      users,
    }).reply(200, { ...manifest, users })
  }

  ping ({ body = {}, responseCode = 200 } = {}) {
    this.nock = this.nock.get('/-/ping?write=true').reply(responseCode, body)
  }

  async package ({ manifest, times = 1, query, tarballs }) {
    let nock = this.nock
    const spec = npa(manifest.name)
    nock = nock.get(`/${spec.escapedName}`).times(times)
    if (query) {
      nock = nock.query(query)
    }
    nock = nock.reply(200, manifest)
    if (tarballs) {
      for (const version in tarballs) {
        const m = manifest.versions[version]
        nock = await this.tarball({ manifest: m, tarball: tarballs[version] })
      }
    }
    this.nock = nock
  }

  async tarball ({ manifest, tarball }) {
    const nock = this.nock
    const dist = new URL(manifest.dist.tarball)
    const tar = await pacote.tarball(tarball)
    nock.get(dist.pathname).reply(200, tar)
    return nock
  }

  // either pass in packuments if you need to set specific attributes besides version,
  // or an array of versions
  // the last packument in the packuments or versions array will be tagged latest
  manifest ({ name = 'test-package', users, packuments, versions } = {}) {
    packuments = this.packuments(packuments, name)
    const latest = packuments.slice(-1)[0]
    const manifest = {
      _id: `${name}@${latest.version}`,
      _rev: '00-testdeadbeef',
      name,
      description: 'test package mock manifest',
      dependencies: {},
      versions: {},
      time: {},
      'dist-tags': { latest: latest.version },
      ...latest,
    }
    if (users) {
      manifest.users = users
    }
    if (versions) {
      packuments = versions.map(version => ({ version }))
    }

    for (const packument of packuments) {
      manifest.versions[packument.version] = {
        _id: `${name}@${packument.version}`,
        name,
        description: 'test package mock manifest',
        dependencies: {},
        dist: {
          tarball: `${this.#registry}/${name}/-/${name}-${packument.version}.tgz`,
        },
        maintainers: [],
        ...packument,
      }
      manifest.time[packument.version] = new Date()
    }

    return manifest
  }

  packuments (packuments = ['1.0.0'], name) {
    return packuments.map(p => this.packument(p, name))
  }

  // Generate packument from shorthand
  packument (packument, name = 'test-package') {
    if (!packument.version) {
      packument = { version: packument }
    }
    return {
      name,
      version: '1.0.0',
      description: 'mocked test package',
      dependencies: {},
      ...packument,
    }
  }
}

module.exports = MockRegistry
