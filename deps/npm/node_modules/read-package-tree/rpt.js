var fs = require('fs')
var rpj = require('read-package-json')
var path = require('path')
var dz = require('dezalgo')
var once = require('once')
var readdir = require('readdir-scoped-modules')
var debug = require('debuglog')('rpt')

function asyncForEach (items, todo, done) {
  var remaining = items.length
  if (remaining === 0) return done()
  var seenErr
  items.forEach(function (item) {
    todo(item, handleComplete)
  })
  function handleComplete (err) {
    if (seenErr) return
    if (err) {
      seenErr = true
      return done(err)
    }
    if (--remaining === 0) done()
  }
}

function dpath (p) {
  if (!p) return ''
  if (p.indexOf(process.cwd()) === 0) {
    p = p.substr(process.cwd().length + 1)
  }
  return p
}

module.exports = rpt

rpt.Node = Node
rpt.Link = Link

var ID = 0
function Node (pkg, logical, physical, er, cache) {
  if (cache[physical]) return cache[physical]

  if (!(this instanceof Node)) {
    return new Node(pkg, logical, physical, er, cache)
  }

  cache[physical] = this

  debug(this.constructor.name, dpath(physical), pkg && pkg._id)

  this.id = ID++
  this.package = pkg || {}
  this.path = logical
  this.realpath = physical
  this.parent = null
  this.isLink = false
  this.children = []
  this.error = er
}

Node.prototype.package = null
Node.prototype.path = ''
Node.prototype.realpath = ''
Node.prototype.children = null
Node.prototype.error = null

function Link (pkg, logical, physical, realpath, er, cache) {
  if (cache[physical]) return cache[physical]

  if (!(this instanceof Link)) {
    return new Link(pkg, logical, physical, realpath, er, cache)
  }

  cache[physical] = this

  debug(this.constructor.name, dpath(physical), pkg && pkg._id)

  this.id = ID++
  this.path = logical
  this.realpath = realpath
  this.package = pkg || {}
  this.parent = null
  this.target = new Node(this.package, logical, realpath, er, cache)
  this.isLink = true
  this.children = this.target.children
  this.error = er
}

Link.prototype = Object.create(Node.prototype, {
  constructor: { value: Link }
})
Link.prototype.target = null
Link.prototype.realpath = ''

function loadNode (logical, physical, cache, cb) {
  debug('loadNode', dpath(logical))
  return fs.realpath(physical, thenReadPackageJson)

  var realpath
  function thenReadPackageJson (er, real) {
    if (er) {
      var node = new Node(null, logical, physical, er, cache)
      return cb(null, node)
    }
    debug('realpath l=%j p=%j real=%j', dpath(logical), dpath(physical), dpath(real))
    var pj = path.join(real, 'package.json')
    realpath = real
    return rpj(pj, thenCreateNode)
  }
  function thenCreateNode (er, pkg) {
    pkg = pkg || null
    var node
    if (physical === realpath) {
      node = new Node(pkg, logical, physical, er, cache)
    } else {
      node = new Link(pkg, logical, physical, realpath, er, cache)
    }

    cb(null, node)
  }
}

function loadChildren (node, cache, filterWith, cb) {
  debug('loadChildren', dpath(node.path))
  // needed 'cause we process all kids async-like and errors
  // short circuit, so we have to be sure that after an error
  // the cbs from other kids don't result in calling cb a second
  // (or more) time.
  cb = once(cb)
  var nm = path.join(node.path, 'node_modules')
  var rm
  return fs.realpath(path.join(node.path, 'node_modules'), thenReaddir)

  function thenReaddir (er, real_nm) {
    if (er) return cb(null, node)
    rm = real_nm
    readdir(nm, thenLoadKids)
  }

  function thenLoadKids (er, kids) {
    // If there are no children, that's fine, just return
    if (er) return cb(null, node)

    kids = kids.filter(function (kid) {
      return kid[0] !== '.' && (!filterWith || filterWith(node, kid))
    })

    asyncForEach(kids, thenLoadNode, thenSortChildren)
  }
  function thenLoadNode (kid, done) {
    var kidPath = path.join(nm, kid)
    var kidRealPath = path.join(rm, kid)
    loadNode(kidPath, kidRealPath, cache, andAddNode(done))
  }
  function andAddNode (done) {
    return function (er, kid) {
      if (er) return done(er)
      node.children.push(kid)
      kid.parent = node
      done()
    }
  }
  function thenSortChildren (er) {
    sortChildren(node)
    cb(er, node)
  }
}

function sortChildren (node) {
  node.children = node.children.sort(function (a, b) {
    a = a.package.name ? a.package.name.toLowerCase() : a.path
    b = b.package.name ? b.package.name.toLowerCase() : b.path
    return a > b ? 1 : -1
  })
}

function loadTree (node, did, cache, filterWith, cb) {
  debug('loadTree', dpath(node.path), !!cache[node.path])

  if (did[node.realpath]) {
    return dz(cb)(null, node)
  }

  did[node.realpath] = true

  // needed 'cause we process all kids async-like and errors
  // short circuit, so we have to be sure that after an error
  // the cbs from other kids don't result in calling cb a second
  // (or more) time.
  cb = once(cb)
  return loadChildren(node, cache, filterWith, thenProcessChildren)

  function thenProcessChildren (er, node) {
    if (er) return cb(er)

    var kids = node.children.filter(function (kid) {
      return !did[kid.realpath]
    })

    return asyncForEach(kids, loadTreeForKid, cb)
  }
  function loadTreeForKid (kid, done) {
    loadTree(kid, did, cache, filterWith, done)
  }
}

function rpt (root, filterWith, cb) {
  if (!cb) {
    cb = filterWith
    filterWith = null
  }
  var cache = Object.create(null)
  var topErr
  var tree
  return fs.realpath(root, thenLoadNode)

  function thenLoadNode (er, realRoot) {
    if (er) return cb(er)
    debug('rpt', dpath(realRoot))
    loadNode(root, realRoot, cache, thenLoadTree)
  }
  function thenLoadTree(er, node) {
    // even if there's an error, it's fine, as long as we got a node
    if (node) {
      topErr = er
      tree = node
      loadTree(node, {}, cache, filterWith, thenHandleErrors)
    } else {
      cb(er)
    }
  }
  function thenHandleErrors (er) {
    cb(topErr && topErr.code !== 'ENOENT' ? topErr : er, tree)
  }
}
