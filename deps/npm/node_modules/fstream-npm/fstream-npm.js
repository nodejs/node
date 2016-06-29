var Ignore = require('fstream-ignore')
var inherits = require('inherits')
var path = require('path')
var fs = require('fs')

module.exports = Packer

inherits(Packer, Ignore)

function Packer (props) {
  if (!(this instanceof Packer)) {
    return new Packer(props)
  }

  if (typeof props === 'string') {
    props = { path: props }
  }

  props.ignoreFiles = props.ignoreFiles || [ '.npmignore',
                                             '.gitignore',
                                             'package.json' ]

  Ignore.call(this, props)

  this.bundled = props.bundled
  this.bundleLinks = props.bundleLinks
  this.package = props.package

  // only do the magic bundling stuff for the node_modules folder that
  // lives right next to a package.json file.
  this.bundleMagic = this.parent &&
                     this.parent.packageRoot &&
                     this.basename === 'node_modules'

  // in a node_modules folder, resolve symbolic links to
  // bundled dependencies when creating the package.
  props.follow = this.follow = this.bundleMagic
  // console.error("follow?", this.path, props.follow)

  if (this === this.root ||
      this.parent &&
      this.parent.bundleMagic &&
      this.basename.charAt(0) !== '.') {
    this.readBundledLinks()
  }

  this.on('entryStat', function (entry, props) {
    // files should *always* get into tarballs
    // in a user-writable state, even if they're
    // being installed from some wackey vm-mounted
    // read-only filesystem.
    entry.mode = props.mode = props.mode | parseInt('0200', 8)
  })
}

Packer.prototype.readBundledLinks = function () {
  if (this._paused) {
    this.once('resume', this.addIgnoreFiles)
    return
  }

  this.pause()
  fs.readdir(this.path + '/node_modules', function (er, list) {
    // no harm if there's no bundle
    var l = list && list.length
    if (er || l === 0) return this.resume()

    var errState = null
    var then = function then (er) {
      if (errState) return
      if (er) {
        errState = er
        return this.resume()
      }
      if (--l === 0) return this.resume()
    }.bind(this)

    list.forEach(function (pkg) {
      if (pkg.charAt(0) === '.') return then()
      var pd = this.path + '/node_modules/' + pkg
      fs.realpath(pd, function (er, rp) {
        if (er) return then()
        this.bundleLinks = this.bundleLinks || {}
        this.bundleLinks[pkg] = rp
        then()
      }.bind(this))
    }, this)
  }.bind(this))
}

Packer.prototype.applyIgnores = function (entry, partial, entryObj) {
  if (!entryObj || entryObj.type !== 'Directory') {
    // package.json files can never be ignored.
    if (entry === 'package.json') return true

    // readme files should never be ignored.
    if (entry.match(/^readme(\.[^\.]*)$/i)) return true

    // license files should never be ignored.
    if (entry.match(/^(license|licence)(\.[^\.]*)?$/i)) return true

    // copyright notice files should never be ignored.
    if (entry.match(/^(notice)(\.[^\.]*)?$/i)) return true

    // changelogs should never be ignored.
    if (entry.match(/^(changes|changelog|history)(\.[^\.]*)?$/i)) return true
  }

  // special rules.  see below.
  if (entry === 'node_modules' && this.packageRoot) return true

  // package.json main file should never be ignored.
  var mainFile = this.package && this.package.main
  if (mainFile && path.resolve(this.path, entry) === path.resolve(this.path, mainFile)) return true

  // some files are *never* allowed under any circumstances
  // (VCS folders, native build cruft, npm cruft, regular cruft)
  if (entry === '.git' ||
      entry === 'CVS' ||
      entry === '.svn' ||
      entry === '.hg' ||
      entry === '.lock-wscript' ||
      entry.match(/^\.wafpickle-[0-9]+$/) ||
      (this.parent && this.parent.packageRoot && this.basename === 'build' &&
       entry === 'config.gypi') ||
      entry === 'npm-debug.log' ||
      entry === '.npmrc' ||
      entry.match(/^\..*\.swp$/) ||
      entry === '.DS_Store' ||
      entry.match(/^\._/)
    ) {
    return false
  }

  // in a node_modules folder, we only include bundled dependencies
  // also, prevent packages in node_modules from being affected
  // by rules set in the containing package, so that
  // bundles don't get busted.
  // Also, once in a bundle, everything is installed as-is
  // To prevent infinite cycles in the case of cyclic deps that are
  // linked with npm link, even in a bundle, deps are only bundled
  // if they're not already present at a higher level.
  if (this.bundleMagic) {
    // bubbling up.  stop here and allow anything the bundled pkg allows
    if (entry.indexOf('/') !== -1) return true

    // never include the .bin.  It's typically full of platform-specific
    // stuff like symlinks and .cmd files anyway.
    if (entry === '.bin') return false

    // the package root.
    var p = this.parent
    // the package before this one.
    var pp = p && p.parent

    // if this entry has already been bundled, and is a symlink,
    // and it is the *same* symlink as this one, then exclude it.
    if (pp && pp.bundleLinks && this.bundleLinks &&
        pp.bundleLinks[entry] &&
        pp.bundleLinks[entry] === this.bundleLinks[entry]) {
      return false
    }

    // since it's *not* a symbolic link, if we're *already* in a bundle,
    // then we should include everything.
    if (pp && pp.package && pp.basename === 'node_modules') {
      return true
    }

    // only include it at this point if it's a bundleDependency
    var bd = this.package && this.package.bundleDependencies

    if (bd && !Array.isArray(bd)) {
      throw new Error(this.package.name + '\'s `bundledDependencies` should ' +
                      'be an array')
    }

    var shouldBundle = bd && bd.indexOf(entry) !== -1
    // if we're not going to bundle it, then it doesn't count as a bundleLink
    // if (this.bundleLinks && !shouldBundle) delete this.bundleLinks[entry]
    return shouldBundle
  }
  // if (this.bundled) return true

  return Ignore.prototype.applyIgnores.call(this, entry, partial, entryObj)
}

Packer.prototype.addIgnoreFiles = function () {
  var entries = this.entries
  // if there's a .npmignore, then we do *not* want to
  // read the .gitignore.
  if (entries.indexOf('.npmignore') !== -1) {
    var i = entries.indexOf('.gitignore')
    if (i !== -1) {
      entries.splice(i, 1)
    }
  }

  this.entries = entries

  Ignore.prototype.addIgnoreFiles.call(this)
}

Packer.prototype.readRules = function (buf, e) {
  if (e !== 'package.json') {
    return Ignore.prototype.readRules.call(this, buf, e)
  }

  buf = buf.toString().trim()

  if (buf.length === 0) return []

  try {
    var p = this.package = JSON.parse(buf)
  } catch (er) {
    // just pretend it's a normal old file, not magic at all.
    return []
  }

  if (this === this.root) {
    this.bundleLinks = this.bundleLinks || {}
    this.bundleLinks[p.name] = this._path
  }

  this.packageRoot = true
  this.emit('package', p)

  // make bundle deps predictable
  if (p.bundledDependencies && !p.bundleDependencies) {
    p.bundleDependencies = p.bundledDependencies
    delete p.bundledDependencies
  }

  if (!p.files || !Array.isArray(p.files)) return []

  // ignore everything except what's in the files array.
  return ['*'].concat(p.files.map(function (f) {
    return '!' + f
  })).concat(p.files.map(function (f) {
    return '!' + f.replace(/\/+$/, '') + '/**'
  }))
}

Packer.prototype.getChildProps = function (stat) {
  var props = Ignore.prototype.getChildProps.call(this, stat)

  props.package = this.package

  props.bundled = this.bundled && this.bundled.slice(0)
  props.bundleLinks = this.bundleLinks &&
    Object.create(this.bundleLinks)

  // Directories have to be read as Packers
  // otherwise fstream.Reader will create a DirReader instead.
  if (stat.isDirectory()) {
    props.type = this.constructor
  }

  // only follow symbolic links directly in the node_modules folder.
  props.follow = false
  return props
}

var order = [
  'package.json',
  '.npmignore',
  '.gitignore',
  /^README(\.md)?$/,
  'LICENCE',
  'LICENSE',
  /\.js$/
]

Packer.prototype.sort = function (a, b) {
  for (var i = 0, l = order.length; i < l; i++) {
    var o = order[i]
    if (typeof o === 'string') {
      if (a === o) return -1
      if (b === o) return 1
    } else {
      if (a.match(o)) return -1
      if (b.match(o)) return 1
    }
  }

  // deps go in the back
  if (a === 'node_modules') return 1
  if (b === 'node_modules') return -1

  return Ignore.prototype.sort.call(this, a, b)
}

Packer.prototype.emitEntry = function (entry) {
  if (this._paused) {
    this.once('resume', this.emitEntry.bind(this, entry))
    return
  }

  // if there is a .gitignore, then we're going to
  // rename it to .npmignore in the output.
  if (entry.basename === '.gitignore') {
    entry.basename = '.npmignore'
    entry.path = path.resolve(entry.dirname, entry.basename)
  }

  // all *.gyp files are renamed to binding.gyp for node-gyp
  // but only when they are in the same folder as a package.json file.
  if (entry.basename.match(/\.gyp$/) &&
      this.entries.indexOf('package.json') !== -1) {
    entry.basename = 'binding.gyp'
    entry.path = path.resolve(entry.dirname, entry.basename)
  }

  // skip over symbolic links
  if (entry.type === 'SymbolicLink') {
    entry.abort()
    return
  }

  if (entry.type !== 'Directory') {
    // make it so that the folder in the tarball is named "package"
    var h = path.dirname((entry.root || entry).path)
    var t = entry.path.substr(h.length + 1).replace(/^[^\/\\]+/, 'package')
    var p = h + '/' + t

    entry.path = p
    entry.dirname = path.dirname(p)
    return Ignore.prototype.emitEntry.call(this, entry)
  }

  // we don't want empty directories to show up in package
  // tarballs.
  // don't emit entry events for dirs, but still walk through
  // and read them.  This means that we need to proxy up their
  // entry events so that those entries won't be missed, since
  // .pipe() doesn't do anythign special with "child" events, on
  // with "entry" events.
  var me = this
  entry.on('entry', function (e) {
    if (e.parent === entry) {
      e.parent = me
      me.emit('entry', e)
    }
  })
  entry.on('package', this.emit.bind(this, 'package'))
}
