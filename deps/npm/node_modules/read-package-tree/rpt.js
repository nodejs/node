var fs = require('fs')
var rpj = require('read-package-json')
var path = require('path')
var dz = require('dezalgo')
var once = require('once')
var readdir = require('readdir-scoped-modules')
var debug = require('debuglog')('rpt')

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
  fs.realpath(physical, function (er, real) {
    if (er) return cb(er)
    debug('realpath l=%j p=%j real=%j', dpath(logical), dpath(physical), dpath(real))
    var pj = path.resolve(real, 'package.json')
    rpj(pj, function (er, pkg) {
      pkg = pkg || null
      var node
      if (physical === real) {
        node = new Node(pkg, logical, physical, er, cache)
      } else {
        node = new Link(pkg, logical, physical, real, er, cache)
      }

      cb(null, node)
    })
  })
}

function loadChildren (node, cache, filterWith, cb) {
  debug('loadChildren', dpath(node.path))
  // don't let it be called more than once
  cb = once(cb)
  var nm = path.resolve(node.path, 'node_modules')
  readdir(nm, function (er, kids) {
    // If there are no children, that's fine, just return
    if (er) return cb(null, node)

    kids = kids.filter(function (kid) {
      return kid[0] !== '.' && (!filterWith || filterWith(node, kid))
    })

    var l = kids . length
    if (l === 0) return cb(null, node)

    kids.forEach(function (kid) {
      var kidPath = path.resolve(nm, kid)
      var kidRealPath = path.resolve(node.realpath,'node_modules',kid)
      loadNode(kidPath, kidRealPath, cache, then)
    })

    function then (er, kid) {
      if (er) return cb(er)

      node.children.push(kid)
      kid.parent = node
      if (--l === 0) {
        sortChildren(node)
        return cb(null, node)
      }
    }
  })
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

  cb = once(cb)
  loadChildren(node, cache, filterWith, function (er, node) {
    if (er) return cb(er)

    var kids = node.children.filter(function (kid) {
      return !did[kid.realpath]
    })

    var l = kids.length
    if (l === 0) return cb(null, node)

    kids.forEach(function (kid, index) {
      loadTree(kid, did, cache, filterWith, then)
    })

    function then (er, kid) {
      if (er) return cb(er)

      if (--l === 0) cb(null, node)
    }
  })
}

function rpt (root, filterWith, cb) {
  if (!cb) {
    cb = filterWith
    filterWith = null
  }
  fs.realpath(root, function (er, realRoot) {
    if (er) return cb(er)
    debug('rpt', dpath(realRoot))
    var cache = Object.create(null)
    loadNode(root, realRoot, cache, function (er, node) {
      // if there's an error, it's fine, as long as we got a node
      if (!node) return cb(er)
      loadTree(node, {}, cache, filterWith, function (lter, tree) {
        cb(er && er.code !== 'ENOENT' ? er : lter, tree)
      })
    })
  })
}
