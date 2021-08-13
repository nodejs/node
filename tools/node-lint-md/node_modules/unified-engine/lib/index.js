/**
 * @typedef {import('vfile').VFile} VFile
 * @typedef {import('unified').Processor<any, any, any, any>} Processor
 * @typedef {import('./file-set.js').FileSet} FileSet
 * @typedef {import('./file-set.js').Completer} Completer
 * @typedef {import('./ignore.js').ResolveFrom} ResolveFrom
 * @typedef {import('./configuration.js').ConfigTransform} ConfigTransform
 * @typedef {import('./configuration.js').Preset} Preset
 *
 * @typedef VFileReporterFields
 * @property {boolean} [color]
 * @property {boolean} [quiet]
 * @property {boolean} [silent]
 *
 * @typedef {{[key: string]: unknown} & VFileReporterFields} VFileReporterOptions
 *
 * @callback VFileReporter
 * @param {VFile[]} files
 * @param {VFileReporterOptions} options
 * @returns {string}
 *
 * @typedef Settings
 * @property {Options['processor']} processor
 * @property {Exclude<Options['cwd'], undefined>} cwd
 * @property {Exclude<Options['files'], undefined>} files
 * @property {Exclude<Options['extensions'], undefined>} extensions
 * @property {Exclude<Options['streamIn'], undefined>} streamIn
 * @property {Options['filePath']} filePath
 * @property {Exclude<Options['streamOut'], undefined>} streamOut
 * @property {Exclude<Options['streamError'], undefined>} streamError
 * @property {Options['out']} out
 * @property {Options['output']} output
 * @property {Options['alwaysStringify']} alwaysStringify
 * @property {Options['tree']} tree
 * @property {Options['treeIn']} treeIn
 * @property {Options['treeOut']} treeOut
 * @property {Options['inspect']} inspect
 * @property {Options['rcName']} rcName
 * @property {Options['packageField']} packageField
 * @property {Options['detectConfig']} detectConfig
 * @property {Options['rcPath']} rcPath
 * @property {Exclude<Options['settings'], undefined>} settings
 * @property {Options['ignoreName']} ignoreName
 * @property {Options['detectIgnore']} detectIgnore
 * @property {Options['ignorePath']} ignorePath
 * @property {Options['ignorePathResolveFrom']} ignorePathResolveFrom
 * @property {Exclude<Options['ignorePatterns'], undefined>} ignorePatterns
 * @property {Options['silentlyIgnore']} silentlyIgnore
 * @property {Options['plugins']} plugins
 * @property {Options['pluginPrefix']} pluginPrefix
 * @property {Options['configTransform']} configTransform
 * @property {Options['defaultConfig']} defaultConfig
 * @property {Options['reporter']} reporter
 * @property {Options['reporterOptions']} reporterOptions
 * @property {Options['color']} color
 * @property {Options['silent']} silent
 * @property {Options['quiet']} quiet
 * @property {Options['frail']} frail
 *
 * @typedef Options
 *   Options for unified engine
 * @property {() => Processor} processor
 *   Unified processor to transform files
 * @property {string} [cwd]
 *   Directory to search files in, load plugins from, and more.
 *   Defaults to `process.cwd()`.
 * @property {Array<string|VFile>} [files]
 *   Paths or globs to files and directories, or virtual files, to process.
 * @property {string[]} [extensions]
 *   If `files` matches directories, include `files` with `extensions`
 * @property {NodeJS.ReadableStream} [streamIn]
 *   Stream to read from if no files are found or given.
 *   Defaults to `process.stdin`.
 * @property {string} [filePath]
 *   File path to process the given file on `streamIn` as.
 * @property {NodeJS.WritableStream} [streamOut]
 *   Stream to write processed files to.
 *   Defaults to `process.stdout`.
 * @property {NodeJS.WritableStream} [streamError]
 *   Stream to write the report (if any) to.
 *   Defaults to `process.stderr`.
 * @property {boolean} [out=false]
 *   Whether to write the processed file to `streamOut`
 * @property {boolean|string} [output=false]
 *   Whether to write successfully processed files, and where to.
 *
 *   * When `true`, overwrites the given files
 *   * When `false`, does not write to the file system
 *   * When pointing to an existing directory, files are written to that
 *     directory and keep their original basenames
 *   * When the parent directory of the given path exists and one file is
 *     processed, the file is written to the given path
 * @property {boolean} [alwaysStringify=false]
 *   Whether to always serialize successfully processed files.
 * @property {boolean} [tree=false]
 *   Whether to treat both input and output as a syntax tree.
 * @property {boolean} [treeIn]
 *   Whether to treat input as a syntax tree.
 *   Defaults to `options.tree`.
 * @property {boolean} [treeOut]
 *   Whether to treat output as a syntax tree.
 *   Defaults to `options.tree`.
 * @property {boolean} [inspect=false]
 *   Whether to output a formatted syntax tree.
 * @property {string} [rcName]
 *   Name of configuration files to load.
 * @property {string} [packageField]
 *   Property at which configuration can be found in `package.json` files
 * @property {boolean} [detectConfig]
 *   Whether to search for configuration files.
 *   Defaults to `true` if `rcName` or `packageField` are given
 * @property {string} [rcPath]
 *   Filepath to a configuration file to load.
 * @property {Preset['settings']} [settings]
 *   Configuration for the parser and compiler of the processor.
 * @property {string} [ignoreName]
 *   Name of ignore files to load.
 * @property {boolean} [detectIgnore]
 *   Whether to search for ignore files.
 *   Defaults to `true` if `ignoreName` is given.
 * @property {string} [ignorePath]
 *   Filepath to an ignore file to load.
 * @property {ResolveFrom} [ignorePathResolveFrom]
 *   Resolve patterns in `ignorePath` from the current working
 *   directory (`'cwd'`) or the ignore file’s directory (`'dir'`, default).
 * @property {string[]} [ignorePatterns]
 *   Patterns to ignore in addition to ignore files
 * @property {boolean} [silentlyIgnore=false]
 *   Skip given files if they are ignored.
 * @property {Preset['plugins']} [plugins]
 *   Plugins to use.
 * @property {string} [pluginPrefix]
 *   Prefix to use when searching for plugins
 * @property {ConfigTransform} [configTransform]
 *   Transform config files from a different schema.
 * @property {Preset} [defaultConfig]
 *   Default configuration to use if no config file is given or found.
 * @property {VFileReporter|string} [reporter]
 *   Reporter to use
 *   Defaults to `vfile-reporter`
 * @property {VFileReporterOptions} [reporterOptions]
 *   Config to pass to the used reporter.
 * @property {VFileReporterOptions['color']} [color=false]
 *   Whether to report with ANSI color sequences.
 * @property {VFileReporterOptions['silent']} [silent=false]
 *   Report only fatal errors
 * @property {VFileReporterOptions['quiet']} [quiet=false]
 *   Do not report successful files
 * @property {boolean} [frail=false]
 *   Call back with an unsuccessful (`1`) code on warnings as well as errors
 *
 * @typedef Context
 *   Processing context.
 * @property {VFile[]} [files]
 *   Processed files.
 * @property {FileSet} [fileSet]
 *   Internally used information
 *
 * @callback Callback
 *   Callback called when processing according to options is complete.
 *   Invoked with either a fatal error if processing went horribly wrong
 *   (probably due to incorrect configuration), or a status code and the
 *   processing context.
 * @param {Error|null} error
 * @param {0|1} [status]
 * @param {Context} [context]
 * @returns {void}
 */

import process from 'node:process'
import {PassThrough} from 'node:stream'
import {statistics} from 'vfile-statistics'
import {fileSetPipeline} from './file-set-pipeline/index.js'

/**
 * Run the file set pipeline once.
 * `callback` is called with a fatal error, or with a status code (`0` on
 * success, `1` on failure).
 *
 * @param {Options} options
 * @param {Callback} callback
 */
export function engine(options, callback) {
  /** @type {Settings} */
  const settings = {}
  /** @type {NodeJS.ReadStream} */
  // @ts-expect-error: `PassThrough` sure is readable.
  let stdin = new PassThrough()

  try {
    stdin = process.stdin
    // Obscure bug in Node (seen on Windows).
    // See: <https://github.com/nodejs/node/blob/f856234/lib/internal/process/stdio.js#L82>,
    // <https://github.com/AtomLinter/linter-markdown/pull/85>.
    /* c8 ignore next 1 */
  } catch {}

  if (!callback) {
    throw new Error('Missing `callback`')
  }

  // Needed `any`s
  // type-coverage:ignore-next-line
  if (!options || !options.processor) {
    return next(new Error('Missing `processor`'))
  }

  // Processor.
  // Needed `any`s
  // type-coverage:ignore-next-line
  settings.processor = options.processor

  // Path to run as.
  settings.cwd = options.cwd || process.cwd()

  // Input.
  settings.files = options.files || []
  settings.extensions = (options.extensions || []).map((ext) =>
    ext.charAt(0) === '.' ? ext : '.' + ext
  )

  settings.filePath = options.filePath
  settings.streamIn = options.streamIn || stdin

  // Output.
  settings.streamOut = options.streamOut || process.stdout
  settings.streamError = options.streamError || process.stderr
  settings.alwaysStringify = options.alwaysStringify
  settings.output = options.output
  settings.out = options.out

  // Null overwrites config settings, `undefined` does not.
  if (settings.output === null || settings.output === undefined) {
    settings.output = undefined
  }

  if (settings.output && settings.out) {
    return next(new Error('Cannot accept both `output` and `out`'))
  }

  // Process phase management.
  const tree = options.tree || false

  settings.treeIn = options.treeIn
  settings.treeOut = options.treeOut
  settings.inspect = options.inspect

  if (settings.treeIn === null || settings.treeIn === undefined) {
    settings.treeIn = tree
  }

  if (settings.treeOut === null || settings.treeOut === undefined) {
    settings.treeOut = tree
  }

  // Configuration.
  const detectConfig = options.detectConfig
  const hasConfig = Boolean(options.rcName || options.packageField)

  if (detectConfig && !hasConfig) {
    return next(
      new Error('Missing `rcName` or `packageField` with `detectConfig`')
    )
  }

  settings.detectConfig =
    detectConfig === null || detectConfig === undefined
      ? hasConfig
      : detectConfig
  settings.rcName = options.rcName
  settings.rcPath = options.rcPath
  settings.packageField = options.packageField
  settings.settings = options.settings || {}
  settings.configTransform = options.configTransform
  settings.defaultConfig = options.defaultConfig

  // Ignore.
  const detectIgnore = options.detectIgnore
  const hasIgnore = Boolean(options.ignoreName)

  settings.detectIgnore =
    detectIgnore === null || detectIgnore === undefined
      ? hasIgnore
      : detectIgnore
  settings.ignoreName = options.ignoreName
  settings.ignorePath = options.ignorePath
  settings.ignorePathResolveFrom = options.ignorePathResolveFrom || 'dir'
  settings.ignorePatterns = options.ignorePatterns || []
  settings.silentlyIgnore = Boolean(options.silentlyIgnore)

  if (detectIgnore && !hasIgnore) {
    return next(new Error('Missing `ignoreName` with `detectIgnore`'))
  }

  // Plugins.
  settings.pluginPrefix = options.pluginPrefix
  settings.plugins = options.plugins || []

  // Reporting.
  settings.reporter = options.reporter
  settings.reporterOptions = options.reporterOptions
  settings.color = options.color || false
  settings.silent = options.silent
  settings.quiet = options.quiet
  settings.frail = options.frail

  // Process.
  fileSetPipeline.run({files: options.files || []}, settings, next)

  /**
   * @param {Error|null} error
   * @param {Context} [context]
   */
  function next(error, context) {
    const stats = statistics((context || {}).files)
    const failed = Boolean(
      settings.frail ? stats.fatal || stats.warn : stats.fatal
    )

    if (error) {
      callback(error)
    } else {
      callback(null, failed ? 1 : 0, context)
    }
  }
}
