/**
 * @typedef {import('unified-engine').Options} EngineOptions
 * @typedef {import('unified-engine').Context} EngineContext
 * @typedef {import('unified-engine').Callback} EngineCallback
 * @typedef {import('./options.js').Options} Options
 */

import process from 'node:process'
import stream from 'node:stream'
import chalk from 'chalk'
import chokidar from 'chokidar'
import {engine} from 'unified-engine'
import {options} from './options.js'

// Fake TTY stream.
const ttyStream = Object.assign(new stream.Readable(), {isTTY: true})

// Exit, lazily, with the correct exit status code.
let exitStatus = 0

process.on('exit', onexit)

// Handle uncaught errors, such as from unexpected async behaviour.
process.on('uncaughtException', fail)

/**
 * Start the CLI.
 *
 * @param {Options} cliConfig
 */
export function args(cliConfig) {
  /** @type {EngineOptions & {help: boolean, helpMessage: string, watch: boolean, version: boolean}} */
  let config
  /** @type {chokidar.FSWatcher|undefined} */
  let watcher
  /** @type {boolean|string|undefined} */
  let output

  try {
    // @ts-expect-error: Close enough.
    config = options(process.argv.slice(2), cliConfig)
  } catch (error) {
    return fail(error, true)
  }

  if (config.help) {
    process.stdout.write(
      [
        'Usage: ' + cliConfig.name + ' [options] [path | glob ...]',
        '',
        '  ' + cliConfig.description,
        '',
        'Options:',
        '',
        config.helpMessage,
        ''
      ].join('\n'),
      noop
    )

    return
  }

  if (config.version) {
    process.stdout.write(cliConfig.version + '\n', noop)

    return
  }

  // Modify `config` for watching.
  if (config.watch) {
    output = config.output

    // Do not read from stdin(4).
    config.streamIn = ttyStream

    // Do not write to stdout(4).
    config.out = false

    process.stderr.write(
      chalk.bold('Watching...') + ' (press CTRL+C to exit)\n',
      noop
    )

    // Prevent infinite loop if set to regeneration.
    if (output === true) {
      config.output = false

      process.stderr.write(
        chalk.yellow('Note') + ': Ignoring `--output` until exit.\n',
        noop
      )
    }
  }

  // Initial run.
  engine(config, done)

  /**
   * Handle complete run.
   *
   * @type {EngineCallback}
   */
  function done(error, code, context) {
    if (error) {
      clean()
      fail(error)
    } else {
      exitStatus = code || 0

      if (config.watch && !watcher && context) {
        subscribe(context)
      }
    }
  }

  // Clean the watcher.
  function clean() {
    if (watcher) {
      watcher.close()
      watcher = undefined
    }
  }

  /**
   * Subscribe a chokidar watcher to all processed files.
   *
   * @param {EngineContext} context
   */
  function subscribe(context) {
    watcher = chokidar
      // @ts-expect-error: `fileSet` is available.
      .watch(context.fileSet.origins, {cwd: config.cwd, ignoreInitial: true})
      .on('error', done)
      .on('change', (filePath) => {
        config.files = [filePath]
        engine(config, done)
      })

    process.on('SIGINT', onsigint)

    /**
     * Handle a SIGINT.
     */
    function onsigint() {
      // Hide the `^C` in terminal.
      process.stderr.write('\n', noop)

      clean()

      // Do another process if `output` specified regeneration.
      if (output === true) {
        config.output = output
        config.watch = false
        engine(config, done)
      }
    }
  }
}

/**
 * Print an error, optionally with stack.
 *
 * @param {Error} error
 * @param {boolean} [pretty=false]
 */
function fail(error, pretty) {
  // Old versions of Node
  /* c8 ignore next 1 */
  const message = String((pretty ? error : error.stack) || error)

  exitStatus = 1

  process.stderr.write(message.trim() + '\n', noop)
}

function onexit() {
  /* eslint-disable unicorn/no-process-exit */
  process.exit(exitStatus)
  /* eslint-enable unicorn/no-process-exit */
}

function noop() {}
