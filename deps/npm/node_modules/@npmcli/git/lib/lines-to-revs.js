// turn an array of lines from `git ls-remote` into a thing
// vaguely resembling a packument, where docs are a resolved ref

const semver = require('semver')

module.exports = lines => finish(lines.reduce(linesToRevsReducer, {
  versions: {},
  'dist-tags': {},
  refs: {},
  shas: {},
}))

const finish = revs => distTags(shaList(peelTags(revs)))

// We can check out shallow clones on specific SHAs if we have a ref
const shaList = revs => {
  Object.keys(revs.refs).forEach(ref => {
    const doc = revs.refs[ref]
    if (!revs.shas[doc.sha]) {
      revs.shas[doc.sha] = [ref]
    } else {
      revs.shas[doc.sha].push(ref)
    }
  })
  return revs
}

// Replace any tags with their ^{} counterparts, if those exist
const peelTags = revs => {
  Object.keys(revs.refs).filter(ref => ref.endsWith('^{}')).forEach(ref => {
    const peeled = revs.refs[ref]
    const unpeeled = revs.refs[ref.replace(/\^\{\}$/, '')]
    if (unpeeled) {
      unpeeled.sha = peeled.sha
      delete revs.refs[ref]
    }
  })
  return revs
}

const distTags = revs => {
  // not entirely sure what situations would result in an
  // ichabod repo, but best to be careful in Sleepy Hollow anyway
  const HEAD = revs.refs.HEAD || /* istanbul ignore next */ {}
  const versions = Object.keys(revs.versions)
  versions.forEach(v => {
    // simulate a dist-tags with latest pointing at the
    // 'latest' branch if one exists and is a version,
    // or HEAD if not.
    const ver = revs.versions[v]
    if (revs.refs.latest && ver.sha === revs.refs.latest.sha) {
      revs['dist-tags'].latest = v
    } else if (ver.sha === HEAD.sha) {
      revs['dist-tags'].HEAD = v
      if (!revs.refs.latest) {
        revs['dist-tags'].latest = v
      }
    }
  })
  return revs
}

const refType = ref => {
  if (ref.startsWith('refs/tags/')) {
    return 'tag'
  }
  if (ref.startsWith('refs/heads/')) {
    return 'branch'
  }
  if (ref.startsWith('refs/pull/')) {
    return 'pull'
  }
  if (ref === 'HEAD') {
    return 'head'
  }
  // Could be anything, ignore for now
  /* istanbul ignore next */
  return 'other'
}

// return the doc, or null if we should ignore it.
const lineToRevDoc = line => {
  const split = line.trim().split(/\s+/, 2)
  if (split.length < 2) {
    return null
  }

  const sha = split[0].trim()
  const rawRef = split[1].trim()
  const type = refType(rawRef)

  if (type === 'tag') {
    // refs/tags/foo^{} is the 'peeled tag', ie the commit
    // that is tagged by refs/tags/foo they resolve to the same
    // content, just different objects in git's data structure.
    // But, we care about the thing the tag POINTS to, not the tag
    // object itself, so we only look at the peeled tag refs, and
    // ignore the pointer.
    // For now, though, we have to save both, because some tags
    // don't have peels, if they were not annotated.
    const ref = rawRef.substr('refs/tags/'.length)
    return { sha, ref, rawRef, type }
  }

  if (type === 'branch') {
    const ref = rawRef.substr('refs/heads/'.length)
    return { sha, ref, rawRef, type }
  }

  if (type === 'pull') {
    // NB: merged pull requests installable with #pull/123/merge
    // for the merged pr, or #pull/123 for the PR head
    const ref = rawRef.substr('refs/'.length).replace(/\/head$/, '')
    return { sha, ref, rawRef, type }
  }

  if (type === 'head') {
    const ref = 'HEAD'
    return { sha, ref, rawRef, type }
  }

  // at this point, all we can do is leave the ref un-munged
  return { sha, ref: rawRef, rawRef, type }
}

const linesToRevsReducer = (revs, line) => {
  const doc = lineToRevDoc(line)

  if (!doc) {
    return revs
  }

  revs.refs[doc.ref] = doc
  revs.refs[doc.rawRef] = doc

  if (doc.type === 'tag') {
    // try to pull a semver value out of tags like `release-v1.2.3`
    // which is a pretty common pattern.
    const match = !doc.ref.endsWith('^{}') &&
      doc.ref.match(/v?(\d+\.\d+\.\d+(?:[-+].+)?)$/)
    if (match && semver.valid(match[1], true)) {
      revs.versions[semver.clean(match[1], true)] = doc
    }
  }

  return revs
}
