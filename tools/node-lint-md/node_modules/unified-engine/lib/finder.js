/**
 * @typedef {import('fs').Stats} Stats
 * @typedef {import('vfile').VFile} VFile
 * @typedef {import('./ignore.js').Ignore} Ignore
 * @typedef {import('ignore').Ignore} GitIgnore
 *
 * @typedef Options
 * @property {string} cwd
 * @property {Array.<string>} extensions
 * @property {boolean|undefined} silentlyIgnore
 * @property {Array.<string>} ignorePatterns
 * @property {Ignore} ignore
 *
 * @typedef SearchResults
 * @property {fs.Stats|undefined} stats
 * @property {boolean|undefined} ignored
 *
 * @typedef Result
 * @property {Array.<string|VFile>} input
 * @property {VFile[]} output
 *
 * @typedef CleanResult
 * @property {boolean} oneFileMode
 * @property {VFile[]} files
 *
 * @callback Callback
 * @param {Error|null} error
 * @param {CleanResult} [result]
 */

import path from 'node:path'
import fs from 'node:fs'
import ignore from 'ignore'
import glob from 'glob'
import {toVFile} from 'to-vfile'

/**
 * Search `patterns`, a mix of globs, paths, and files.
 *
 * @param {Array.<string|VFile>} input
 * @param {Options} options
 * @param {Callback} callback
 */
export function finder(input, options, callback) {
  expand(input, options, (error, result) => {
    // Glob errors are unusual.
    // other errors are on the vfile results.
    /* c8 ignore next 2 */
    if (error || !result) {
      callback(error)
    } else {
      callback(null, {oneFileMode: oneFileMode(result), files: result.output})
    }
  })
}

/**
 * Expand the given glob patterns, search given and found directories, and map
 * to vfiles.
 *
 * @param {Array.<string|VFile>} input
 * @param {Options} options
 * @param {(error: Error|null, result?: Result) => void} next
 */
function expand(input, options, next) {
  /** @type {Array.<string|VFile>} */
  let paths = []
  let actual = 0
  let expected = 0
  let index = -1
  /** @type {boolean|undefined} */
  let failed

  while (++index < input.length) {
    let file = input[index]
    if (typeof file === 'string') {
      if (glob.hasMagic(file)) {
        expected++
        glob(file, {cwd: options.cwd}, (error, files) => {
          // Glob errors are unusual.
          /* c8 ignore next 3 */
          if (failed) {
            return
          }

          // Glob errors are unusual.
          /* c8 ignore next 4 */
          if (error) {
            failed = true
            done1(error)
          } else {
            actual++
            paths = paths.concat(files)

            if (actual === expected) {
              search(paths, options, done1)
            }
          }
        })
      } else {
        // `relative` to make the paths canonical.
        file =
          path.relative(options.cwd, path.resolve(options.cwd, file)) || '.'
        paths.push(file)
      }
    } else {
      const fp = file.path ? path.relative(options.cwd, file.path) : options.cwd
      file.cwd = options.cwd
      file.path = fp
      file.history = [fp]
      paths.push(file)
    }
  }

  if (!expected) {
    search(paths, options, done1)
  }

  /**
   * @param {Error|null} error
   * @param {Array<VFile>} [files]
   */
  function done1(error, files) {
    // `search` currently does not give errors.
    /* c8 ignore next 2 */
    if (error || !files) {
      next(error)
    } else {
      next(null, {input: paths, output: files})
    }
  }
}

/**
 * Search `paths`.
 *
 * @param {Array.<string|VFile>} input
 * @param {Options & {nested?: boolean}} options
 * @param {(error: Error|null, files: Array.<VFile>) => void} next
 */
function search(input, options, next) {
  const extraIgnore = ignore().add(options.ignorePatterns)
  let expected = 0
  let actual = 0
  let index = -1
  /** @type {Array.<VFile>} */
  let files = []

  while (++index < input.length) {
    each(input[index])
  }

  if (!expected) {
    next(null, files)
  }

  /**
   * @param {string|VFile} file
   */
  function each(file) {
    const ext = typeof file === 'string' ? path.extname(file) : file.extname

    // Normalise globs.
    if (typeof file === 'string') {
      file = file.split('/').join(path.sep)
    }

    const part = base(file)

    if (options.nested && (part.charAt(0) === '.' || part === 'node_modules')) {
      return
    }

    expected++

    statAndIgnore(
      file,
      Object.assign({}, options, {extraIgnore}),
      (error, result) => {
        const ignored = result && result.ignored
        const dir = result && result.stats && result.stats.isDirectory()

        if (ignored && (options.nested || options.silentlyIgnore)) {
          return one(null, [])
        }

        if (!ignored && dir) {
          return fs.readdir(
            path.resolve(options.cwd, filePath(file)),
            (error, basenames) => {
              // Should not happen often: the directory is `stat`ed first, which was ok,
              // but reading it is not.
              /* c8 ignore next 9 */
              if (error) {
                const otherFile = toVFile(filePath(file))
                otherFile.cwd = options.cwd

                try {
                  otherFile.fail('Cannot read directory')
                } catch {}

                one(null, [otherFile])
              } else {
                search(
                  basenames.map((name) => path.join(filePath(file), name)),
                  Object.assign({}, options, {nested: true}),
                  one
                )
              }
            }
          )
        }

        if (
          !dir &&
          options.nested &&
          options.extensions.length > 0 &&
          !options.extensions.includes(ext)
        ) {
          return one(null, [])
        }

        file = toVFile(file)
        file.cwd = options.cwd

        if (ignored) {
          try {
            file.fail('Cannot process specified file: it’s ignored')
            // C8 bug on Node@12
            /* c8 ignore next 1 */
          } catch {}
        }

        if (error && error.code === 'ENOENT') {
          try {
            file.fail(
              error.syscall === 'stat' ? 'No such file or directory' : error
            )
            // C8 bug on Node@12
            /* c8 ignore next 1 */
          } catch {}
        }

        one(null, [file])
      }
    )

    /**
     * Error is never given. Always given `results`.
     *
     * @param {Error|null} _
     * @param {Array.<VFile>} results
     */
    function one(_, results) {
      /* istanbul ignore else - Always given. */
      if (results) {
        files = files.concat(results)
      }

      actual++

      if (actual === expected) {
        next(null, files)
      }
    }
  }
}

/**
 * @param {VFile|string} file
 * @param {Options & {extraIgnore: GitIgnore}} options
 * @param {(error: NodeJS.ErrnoException|null, result?: SearchResults) => void} callback
 */
function statAndIgnore(file, options, callback) {
  const fp = path.resolve(options.cwd, filePath(file))
  const normal = path.relative(options.cwd, fp)
  let expected = 1
  let actual = 0
  /** @type {Stats|undefined} */
  let stats
  /** @type {boolean|undefined} */
  let ignored

  if (typeof file === 'string' || !file.value) {
    expected++
    fs.stat(fp, (error, value) => {
      stats = value
      onStartOrCheck(error)
    })
  }

  options.ignore.check(fp, (error, value) => {
    ignored = value
    onStartOrCheck(error)
  })

  /**
   * @param {Error|null} error
   */
  function onStartOrCheck(error) {
    actual++

    if (error) {
      callback(error)
      actual = -1
    } else if (actual === expected) {
      callback(null, {
        stats,
        ignored:
          ignored ||
          (normal === '' ||
          normal === '..' ||
          normal.charAt(0) === path.sep ||
          normal.slice(0, 3) === '..' + path.sep
            ? false
            : options.extraIgnore.ignores(normal))
      })
    }
  }
}

/**
 * @param {string|VFile} file
 * @returns {string}
 */
function base(file) {
  return typeof file === 'string' ? path.basename(file) : file.basename
}

/**
 * @param {string|VFile} file
 * @returns {string}
 */
function filePath(file) {
  return typeof file === 'string' ? file : file.path
}

/**
 * @param {Result} result
 * @returns {boolean}
 */
function oneFileMode(result) {
  return (
    result.output.length === 1 &&
    result.input.length === 1 &&
    result.output[0].path === result.input[0]
  )
}
