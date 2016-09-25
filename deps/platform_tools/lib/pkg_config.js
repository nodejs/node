'use strict';
const path = require('path')
const fs = require('fs')
const readline = require('readline')
const debuglog = require('util').debuglog('platform_tools')

const pkgConfigSearchPaths = [
  '/usr/lib/pkgconfig',
  '/usr/share/pkgconfig',
  '/usr/local/lib/pkgconfig',
  '/usr/local/share/pkgconfig'
]

exports.pkgConfigSearchPaths = pkgConfigSearchPaths

function parsePcLine(line) {
  // brute force REVIEW
  // Rationale:
  //  * match line if it has ': ' or '=' delimiter via split
  //  * both cases get first result as key for the result
  //  * Libs, Cflags, Requires (all colon-case) are special cases, since they
  //    will have multiple values that are space delimited
  //    * those will be split up into an array and checked against, if the array
  //      has empty elements and hence those exluded if so
  //
  const hasColon = line.split(': ')
  const hasEqual= line.split('=')

  let key
  let values
  if (hasColon.length > 1) {
    key = hasColon[0]
    values = hasColon[1]
  } else if (hasEqual.length > 1) {
    key = hasEqual[0]
    values = hasEqual[1]
  }

  if (key === 'Libs' || key === 'Cflags' || key === 'Requires') {
    values = values.split(' ')
    values = values.filter((val) => {
      if (!(val === '')) return val
    })
  }

  return {
    key: key,
    values: values
  }
}

function resolveVariables(result) {
  // brute force REVIEW
  // Rationale:
  //  * check if current result is in array or string format
  //  * for both take the (element-level) string and check if it has
  //    a variable include (checks only for one REVIEW)
  //  * string replace include with the information as key of the result object,
  //    which assumes that the object has a declartion of that variable already
  //    * not recursive
  //
  const re = /(\$\{([^)]+)\})/ // matches ${} brackets and its inside
  for (var key in result) {
    if (typeof result[key] === 'string') {
      let m
      if ((m = re.exec(result[key])) !== null) {
        if (m.index === re.lastIndex)
            re.lastIndex++
        if (m[0])
          result[key] = result[key].replace(m[0], result[m[2]])
      }
    } else if (result[key] && result[key].constructor === Array) {
      let res = []
      result[key].forEach((el) => {
        let m
        // just regular regex stuff here, REVIEW: verbose
        if ((m = re.exec(el)) !== null) {
          if (m.index === re.lastIndex)
              re.lastIndex++
          if (m[0])
            res.push(el.replace(m[0], result[m[2]])) // insert value with key
          else
           res.push(el) // fall through if regex actually hasn't matched a string
        } else {
          res.push(el) // fall through if regex does not variable include
        }
      })
      result[key] = res
    }
  }
  return result
}

// internal doc taken from man plg-config:
//
//  The pkg-config program is  used  to  retrieve  information  about  installed
// libraries  in  the system.  It is typically used to compile and link against
// one or more libraries.  Here is a typical usage scenario in a Makefile:
//
// program: program.c
//     cc program.c $(pkg-config --cflags --libs gnomeui)
//
// pkg-config retrieves information about packages from special metadata files.
// These  files  are named after the package, and has a .pc extension.  On most
// systems,  pkg-config  looks  in  /usr/lib/pkgconfig,   /usr/share/pkgconfig,
// /usr/local/lib/pkgconfig and /usr/local/share/pkgconfig for these files.  It
// will additionally look in the colon-separated (on  Windows,  semicolon-sepa-
// rated)  list  of  directories  specified  by the PKG_CONFIG_PATH environment
// variable.
//
// The package name specified on the pkg-config command line is defined  to  be
// the  name  of  the  metadata file, minus the .pc extension. If a library can
// install multiple versions simultaneously, it must give each version its  own
// name  (for example, GTK 1.2 might have the package name "gtk+" while GTK 2.0
// has "gtk+-2.0").
//
// In addition to specifying a package name on the command line, the full  path
// to  a  given  .pc  file may be given instead. This allows a user to directly
// query a particular .pc file.
//
exports.config = (lib, cb) => {
  const parseTargets = []
  // template for pkg-config-like output
  let result = {
    prefix:      null,
    exec_prefix: null,
    libdir:      null,
    includedir:  null,
    name:        null,
    description: null,
    version:     null,
    requires:    null, // array
    libs:        null, // array
    cflags:      null  // array
  }

  let searchPaths = pkgConfigSearchPaths
  // if user specifies a path, include into the search paths
  // will be git first in case it is a file. If not, falls back to defaults
  if (lib.indexOf(path.sep) >= 0) {
    const file = path.parse(lib)
    if (file.ext !== '.pc')
      return process.nextTick(() => { cb(new InputError('File must have .pc extension.'))})
    searchPaths.unshift(file.dir)
    lib = file.name
  }

  searchPaths.forEach((pathToLib) => {
    let target = `${pathToLib}${path.sep}${lib}.pc`
    let res
    try {
      res = fs.statSync(target)
    } catch(e) {
      // ignore ENOENTs; check will be done below
    }
    if (res && res.isFile()) parseTargets.push(target)
  })
  // bail when no .pc file was found REVIEW
  if (parseTargets.length === 0)
    return process.nextTick(() => { cb(null, null) })

  if (parseTargets.length > 1)
    debuglog('Found more than one .pc file. Only using first one found')

  const runner = readline.createInterface({ input: fs.createReadStream(parseTargets[0]) })
  runner.on('line', (line) => {
    const res = parsePcLine(line)
    if (res && res.key) {
      result[res.key.toLowerCase()] = res.values
    }
  })
  runner.on('error', (err) => { return process.nextTick(() => { cb(err) }) })
  runner.on('close', () => {
    result = resolveVariables(result)
    return process.nextTick(() => { cb(null, result) })
  })
}
