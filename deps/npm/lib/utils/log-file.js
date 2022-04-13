const os = require('os')
const path = require('path')
const { format, promisify } = require('util')
const rimraf = promisify(require('rimraf'))
const glob = promisify(require('glob'))
const MiniPass = require('minipass')
const fsMiniPass = require('fs-minipass')
const fs = require('@npmcli/fs')
const log = require('./log-shim')

const padZero = (n, length) => n.toString().padStart(length.toString().length, '0')

const _logHandler = Symbol('logHandler')
const _formatLogItem = Symbol('formatLogItem')
const _getLogFilePath = Symbol('getLogFilePath')
const _openLogFile = Symbol('openLogFile')
const _cleanLogs = Symbol('cleanlogs')
const _endStream = Symbol('endStream')
const _isBuffered = Symbol('isBuffered')

class LogFiles {
  // If we write multiple log files we want them all to have the same
  // identifier for sorting and matching purposes
  #logId = null

  // Default to a plain minipass stream so we can buffer
  // initial writes before we know the cache location
  #logStream = null

  // We cap log files at a certain number of log events per file.
  // Note that each log event can write more than one line to the
  // file. Then we rotate log files once this number of events is reached
  #MAX_LOGS_PER_FILE = null

  // Now that we write logs continuously we need to have a backstop
  // here for infinite loops that still log. This is also partially handled
  // by the config.get('max-files') option, but this is a failsafe to
  // prevent runaway log file creation
  #MAX_FILES_PER_PROCESS = null

  #fileLogCount = 0
  #totalLogCount = 0
  #dir = null
  #logsMax = null
  #files = []

  constructor ({
    maxLogsPerFile = 50_000,
    maxFilesPerProcess = 5,
  } = {}) {
    this.#logId = LogFiles.logId(new Date())
    this.#MAX_LOGS_PER_FILE = maxLogsPerFile
    this.#MAX_FILES_PER_PROCESS = maxFilesPerProcess
    this.on()
  }

  static logId (d) {
    return d.toISOString().replace(/[.:]/g, '_')
  }

  static format (count, level, title, ...args) {
    let prefix = `${count} ${level}`
    if (title) {
      prefix += ` ${title}`
    }

    return format(...args)
      .split(/\r?\n/)
      .reduce((lines, line) =>
        lines += prefix + (line ? ' ' : '') + line + os.EOL,
      ''
      )
  }

  on () {
    this.#logStream = new MiniPass()
    process.on('log', this[_logHandler])
  }

  off () {
    process.off('log', this[_logHandler])
    this[_endStream]()
  }

  load ({ dir, logsMax = Infinity } = {}) {
    // dir is user configurable and is required to exist so
    // this can error if the dir is missing or not configured correctly
    this.#dir = dir
    this.#logsMax = logsMax

    // Log stream has already ended
    if (!this.#logStream) {
      return
    }

    log.verbose('logfile', `logs-max:${logsMax} dir:${dir}`)

    // Pipe our initial stream to our new file stream and
    // set that as the new log logstream for future writes
    // if logs max is 0 then the user does not want a log file
    if (this.#logsMax > 0) {
      const initialFile = this[_openLogFile]()
      if (initialFile) {
        this.#logStream = this.#logStream.pipe(initialFile)
      }
    }

    // Kickoff cleaning process, even if we aren't writing a logfile.
    // This is async but it will always ignore the current logfile
    // Return the result so it can be awaited in tests
    return this[_cleanLogs]()
  }

  log (...args) {
    this[_logHandler](...args)
  }

  get files () {
    return this.#files
  }

  get [_isBuffered] () {
    return this.#logStream instanceof MiniPass
  }

  [_endStream] (output) {
    if (this.#logStream) {
      this.#logStream.end(output)
      this.#logStream = null
    }
  }

  [_logHandler] = (level, ...args) => {
    // Ignore pause and resume events since we
    // write everything to the log file
    if (level === 'pause' || level === 'resume') {
      return
    }

    // If the stream is ended then do nothing
    if (!this.#logStream) {
      return
    }

    const logOutput = this[_formatLogItem](level, ...args)

    if (this[_isBuffered]) {
      // Cant do anything but buffer the output if we dont
      // have a file stream yet
      this.#logStream.write(logOutput)
      return
    }

    // Open a new log file if we've written too many logs to this one
    if (this.#fileLogCount >= this.#MAX_LOGS_PER_FILE) {
      // Write last chunk to the file and close it
      this[_endStream](logOutput)
      if (this.#files.length >= this.#MAX_FILES_PER_PROCESS) {
        // but if its way too many then we just stop listening
        this.off()
      } else {
        // otherwise we are ready for a new file for the next event
        this.#logStream = this[_openLogFile]()
      }
    } else {
      this.#logStream.write(logOutput)
    }
  }

  [_formatLogItem] (...args) {
    this.#fileLogCount += 1
    return LogFiles.format(this.#totalLogCount++, ...args)
  }

  [_getLogFilePath] (count = '') {
    return path.resolve(this.#dir, `${this.#logId}-debug-${count}.log`)
  }

  [_openLogFile] () {
    // Count in filename will be 0 indexed
    const count = this.#files.length

    try {
      // Pad with zeros so that our log files are always sorted properly
      // We never want to write files ending in `-9.log` and `-10.log` because
      // log file cleaning is done by deleting the oldest so in this example
      // `-10.log` would be deleted next
      const f = this[_getLogFilePath](padZero(count, this.#MAX_FILES_PER_PROCESS))
      // Some effort was made to make the async, but we need to write logs
      // during process.on('exit') which has to be synchronous. So in order
      // to never drop log messages, it is easiest to make it sync all the time
      // and this was measured to be about 1.5% slower for 40k lines of output
      const logStream = fs.withOwnerSync(
        f,
        () => new fsMiniPass.WriteStreamSync(f, { flags: 'a' }),
        { owner: 'inherit' }
      )
      if (count > 0) {
        // Reset file log count if we are opening
        // after our first file
        this.#fileLogCount = 0
      }
      this.#files.push(logStream.path)
      return logStream
    } catch (e) {
      log.warn('logfile', `could not be created: ${e}`)
    }
  }

  async [_cleanLogs] () {
    // module to clean out the old log files
    // this is a best-effort attempt.  if a rm fails, we just
    // log a message about it and move on.  We do return a
    // Promise that succeeds when we've tried to delete everything,
    // just for the benefit of testing this function properly.

    try {
      const logPath = this[_getLogFilePath]()
      const logGlob = path.join(path.dirname(logPath), path.basename(logPath)
        // tell glob to only match digits
        .replace(/\d/g, '[0123456789]')
        // Handle the old (prior to 8.2.0) log file names which did not have a
        // counter suffix
        .replace(/-\.log$/, '*.log')
      )

      // Always ignore the currently written files
      const files = await glob(logGlob, { ignore: this.#files })
      const toDelete = files.length - this.#logsMax

      if (toDelete <= 0) {
        return
      }

      log.silly('logfile', `start cleaning logs, removing ${toDelete} files`)

      for (const file of files.slice(0, toDelete)) {
        try {
          await rimraf(file)
        } catch (e) {
          log.silly('logfile', 'error removing log file', file, e)
        }
      }
    } catch (e) {
      log.warn('logfile', 'error cleaning log files', e)
    } finally {
      log.silly('logfile', 'done cleaning log files')
    }
  }
}

module.exports = LogFiles
