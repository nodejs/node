var fs = require('fs')
var path = require('path');

module.exports = {
  "name" : prompt('name',
    typeof name === 'undefined'
    ? basename.replace(/^node-|[.-]js$/g, ''): name),
  "version" : prompt('version', typeof version !== "undefined"
                              ? version : '0.0.0'),
  "description" : (function () {
      if (typeof description !== 'undefined' && description) {
        return description
      }
      var value;
      try {
          var src = fs.readFileSync('README.md', 'utf8');
          value = src.split('\n').filter(function (line) {
              return /\s+/.test(line)
                  && line.trim() !== basename.replace(/^node-/, '')
                  && !line.trim().match(/^#/)
              ;
          })[0]
              .trim()
              .replace(/^./, function (c) { return c.toLowerCase() })
              .replace(/\.$/, '')
          ;
      }
      catch (e) {
        try {
          // Wouldn't it be nice if that file mattered?
          var d = fs.readFileSync('.git/description', 'utf8')
        } catch (e) {}
        if (d.trim() && !value) value = d
      }
      return prompt('description', value);
  })(),
  "main" : (function () {
    var f
    try {
      f = fs.readdirSync(dirname).filter(function (f) {
        return f.match(/\.js$/)
      })
      if (f.indexOf('index.js') !== -1)
        f = 'index.js'
      else if (f.indexOf('main.js') !== -1)
        f = 'main.js'
      else if (f.indexOf(basename + '.js') !== -1)
        f = basename + '.js'
      else
        f = f[0]
    } catch (e) {}

    return prompt('entry point', f || 'index.js')
  })(),
  "bin" : function (cb) {
    fs.readdir(dirname + '/bin', function (er, d) {
      // no bins
      if (er) return cb()
      // just take the first js file we find there, or nada
      return cb(null, d.filter(function (f) {
        return f.match(/\.js$/)
      })[0])
    })
  },
  "directories" : function (cb) {
    fs.readdir('.', function (er, dirs) {
      if (er) return cb(er)
      var res = {}
      dirs.forEach(function (d) {
        switch (d) {
          case 'example': case 'examples': return res.example = d
          case 'test': case 'tests': return res.test = d
          case 'doc': case 'docs': return res.doc = d
          case 'man': return res.man = d
        }
      })
      if (Object.keys(res).length === 0) res = undefined
      return cb(null, res)
    })
  },
  "dependencies" : typeof dependencies !== 'undefined' ? dependencies
    : function (cb) {
      fs.readdir('node_modules', function (er, dir) {
        if (er) return cb()
        var deps = {}
        var n = dir.length
        dir.forEach(function (d) {
          if (d.match(/^\./)) return next()
          if (d.match(/^(expresso|mocha|tap|coffee-script|coco|streamline)$/))
            return next()
          fs.readFile('node_modules/' + d + '/package.json', function (er, p) {
            if (er) return next()
            try { p = JSON.parse(p) } catch (e) { return next() }
            if (!p.version) return next()
            deps[d] = '~' + p.version
            return next()
          })
        })
        function next () {
          if (--n === 0) return cb(null, deps)
        }
      })
    },
  "devDependencies" : typeof devDependencies !== 'undefined' ? devDependencies
    : function (cb) {
      // same as dependencies but for dev deps
      fs.readdir('node_modules', function (er, dir) {
        if (er) return cb()
        var deps = {}
        var n = dir.length
        dir.forEach(function (d) {
          if (d.match(/^\./)) return next()
          if (!d.match(/^(expresso|mocha|tap|coffee-script|coco|streamline)$/))
            return next()
          fs.readFile('node_modules/' + d + '/package.json', function (er, p) {
            if (er) return next()
            try { p = JSON.parse(p) } catch (e) { return next() }
            if (!p.version) return next()
            deps[d] = '~' + p.version
            return next()
          })
        })
        function next () {
          if (--n === 0) return cb(null, deps)
        }
      })
    },
  "scripts" : (function () {
    // check to see what framework is in use, if any
    try { var d = fs.readdirSync('node_modules') }
    catch (e) { d = [] }
    var s = typeof scripts === 'undefined' ? {} : scripts

    if (d.indexOf('coffee-script') !== -1)
      s.prepublish = prompt('build command',
                            s.prepublish || 'coffee src/*.coffee -o lib')

    var notest = 'echo "Error: no test specified" && exit 1'
    function tx (test) {
      return test || notest
    }

    if (!s.test || s.test === notest) {
      if (d.indexOf('tap') !== -1)
        s.test = prompt('test command', 'tap test/*.js', tx)
      else if (d.indexOf('expresso') !== -1)
        s.test = prompt('test command', 'expresso test', tx)
      else if (d.indexOf('mocha') !== -1)
        s.test = prompt('test command', 'mocha', tx)
      else
        s.test = prompt('test command', tx)
    }

    return s

  })(),

  "repository" : (function () {
    try { var gconf = fs.readFileSync('.git/config') }
    catch (e) { gconf = null }
    if (gconf) {
      gconf = gconf.split(/\r?\n/)
      var i = gconf.indexOf('[remote "origin"]')
      if (i !== -1) {
        var u = gconf[i + 1]
        if (!u.match(/^\s*url =/)) u = gconf[i + 2]
        if (!u.match(/^\s*url =/)) u = null
        else u = u.replace(/^\s*url = /, '')
      }
      if (u && u.match(/^git@github.com:/))
        u = u.replace(/^git@github.com:/, 'git://github.com/')
    }

    return prompt('git repository', u)
  })(),

  "keywords" : prompt(function (s) {
    if (!s) return undefined
    if (Array.isArray(s)) s = s.join(' ')
    if (typeof s !== 'string') return s
    return s.split(/[\s,]+/)
  }),
  "author" : config['init.author.name']
    ? {
        "name" : config['init.author.name'],
        "email" : config['init.author.email'],
        "url" : config['init.author.url']
      }
    : undefined,
  "license" : prompt('license', 'BSD')
}
