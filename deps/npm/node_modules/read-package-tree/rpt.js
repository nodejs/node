const fs = require('fs')
/* istanbul ignore next */
const promisify = require('util').promisify || require('util-promisify')
const { resolve, basename, dirname, join } = require('path')
const rpj = promisify(require('read-package-json'))
const readdir = promisify(require('readdir-scoped-modules'))
const realpath = require('./realpath.js')

let ID = 0
class Node {
  constructor (pkg, logical, physical, er, cache) {
    // should be impossible.
    const cached = cache.get(physical)
    /* istanbul ignore next */
    if (cached && !cached.then)
      throw new Error('re-creating already instantiated node')

    cache.set(physical, this)

    const parent = basename(dirname(logical))
    if (parent.charAt(0) === '@')
      this.name = `${parent}/${basename(logical)}`
    else
      this.name = basename(logical)
    this.path = logical
    this.realpath = physical
    this.error = er
    this.id = ID++
    this.package = pkg || {}
    this.parent = null
    this.isLink = false
    this.children = []
  }
}

class Link extends Node {
  constructor (pkg, logical, physical, realpath, er, cache) {
    super(pkg, logical, physical, er, cache)

    // if the target has started, but not completed, then
    // a Promise will be in the cache to indicate this.
    const cachedTarget = cache.get(realpath)
    if (cachedTarget && cachedTarget.then)
      cachedTarget.then(node => {
        this.target = node
        this.children = node.children
      })

    this.target = cachedTarget || new Node(pkg, logical, realpath, er, cache)
    this.realpath = realpath
    this.isLink = true
    this.error = er
    this.children = this.target.children
  }
}

// this is the way it is to expose a timing issue which is difficult to
// test otherwise.  The creation of a Node may take slightly longer than
// the creation of a Link that targets it.  If the Node has _begun_ its
// creation phase (and put a Promise in the cache) then the Link will
// get a Promise as its cachedTarget instead of an actual Node object.
// This is not a problem, because it gets resolved prior to returning
// the tree or attempting to load children.  However, it IS remarkably
// difficult to get to happen in a test environment to verify reliably.
// Hence this kludge.
const newNode = (pkg, logical, physical, er, cache) =>
  process.env._TEST_RPT_SLOW_LINK_TARGET_ === '1'
    ? new Promise(res => setTimeout(() =>
      res(new Node(pkg, logical, physical, er, cache)), 10))
    : new Node(pkg, logical, physical, er, cache)

const loadNode = (logical, physical, cache, rpcache, stcache) => {
  // cache temporarily holds a promise placeholder so we
  // don't try to create the same node multiple times.
  // this is very rare to encounter, given the aggressive
  // caching on fs.realpath and fs.lstat calls, but
  // it can happen in theory.
  const cached = cache.get(physical)
  /* istanbul ignore next */
  if (cached)
    return Promise.resolve(cached)

  const p = realpath(physical, rpcache, stcache, 0).then(real =>
    rpj(join(real, 'package.json'))
      .then(pkg => [pkg, null], er => [null, er])
      .then(([pkg, er]) =>
        physical === real ? newNode(pkg, logical, physical, er, cache)
        : new Link(pkg, logical, physical, real, er, cache)
      ),
    // if the realpath fails, don't bother with the rest
    er => new Node(null, logical, physical, er, cache))

  cache.set(physical, p)
  return p
}

const loadChildren = (node, cache, filterWith, rpcache, stcache) => {
  // if a Link target has started, but not completed, then
  // a Promise will be in the cache to indicate this.
  //
  // XXX When we can one day loadChildren on the link *target* instead of
  // the link itself, to match real dep resolution, then we may end up with
  // a node target in the cache that isn't yet done resolving when we get
  // here.  For now, though, this line will never be reached, so it's hidden
  //
  // if (node.then)
  //   return node.then(node => loadChildren(node, cache, filterWith, rpcache, stcache))

  const nm = join(node.path, 'node_modules')
  return realpath(nm, rpcache, stcache, 0)
    .then(rm => readdir(rm).then(kids => [rm, kids]))
    .then(([rm, kids]) => Promise.all(
      kids.filter(kid =>
        kid.charAt(0) !== '.' && (!filterWith || filterWith(node, kid)))
      .map(kid => loadNode(join(nm, kid), join(rm, kid), cache, rpcache, stcache)))
    ).then(kidNodes => {
      kidNodes.forEach(k => k.parent = node)
      node.children.push.apply(node.children, kidNodes.sort((a, b) =>
        (a.package.name ? a.package.name.toLowerCase() : a.path)
        .localeCompare(
          (b.package.name ? b.package.name.toLowerCase() : b.path)
        )))
      return node
    })
    .catch(() => node)
}

const loadTree = (node, did, cache, filterWith, rpcache, stcache) => {
  // impossible except in pathological ELOOP cases
  /* istanbul ignore next */
  if (did.has(node.realpath))
    return Promise.resolve(node)

  did.add(node.realpath)

  // load children on the target, not the link
  return loadChildren(node, cache, filterWith, rpcache, stcache)
    .then(node => Promise.all(
      node.children
        .filter(kid => !did.has(kid.realpath))
        .map(kid => loadTree(kid, did, cache, filterWith, rpcache, stcache))
    )).then(() => node)
}

// XXX Drop filterWith and/or cb in next semver major bump
const rpt = (root, filterWith, cb) => {
  if (!cb && typeof filterWith === 'function') {
    cb = filterWith
    filterWith = null
  }

  const cache = new Map()
  // we can assume that the cwd is real enough
  const cwd = process.cwd()
  const rpcache = new Map([[ cwd, cwd ]])
  const stcache = new Map()
  const p = realpath(root, rpcache, stcache, 0)
    .then(realRoot => loadNode(root, realRoot, cache, rpcache, stcache))
    .then(node => loadTree(node, new Set(), cache, filterWith, rpcache, stcache))

  if (typeof cb === 'function')
    p.then(tree => cb(null, tree), cb)

  return p
}

rpt.Node = Node
rpt.Link = Link
module.exports = rpt
