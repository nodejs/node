const os = require('os')
const { join, dirname, basename } = require('path')
const { format } = require('util')
const { glob } = require('glob')
const { Minipass } = require('minipass')
const fsMiniPass = require('fs-minipass')
const fs = require('fs/promises')
const log = require('./log-shim')

const padZero = (n, length) => n.toString().padStart(length.toString().length, '0')
const globify = pattern => pattern.split('\\').join('/')

class LogFiles {
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
  #path = null
  #logsMax = null
  #files = []

  constructor ({
    maxLogsPerFile = 50_000,
    maxFilesPerProcess = 5,
  } = {}) {
    this.#MAX_LOGS_PER_FILE = maxLogsPerFile
    this.#MAX_FILES_PER_PROCESS = maxFilesPerProcess
    this.on()
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
    this.#logStream = new Minipass()
    process.on('log', this.#logHandler)
  }

  off () {
    process.off('log', this.#logHandler)
    this.#endStream()
  }

  load ({ path, logsMax = Infinity } = {}) {
    // dir is user configurable and is required to exist so
    // this can error if the dir is missing or not configured correctly
    this.#path = path
    this.#logsMax = logsMax

    // Log stream has already ended
    if (!this.#logStream) {
      return
    }

    log.verbose('logfile', `logs-max:${logsMax} dir:${this.#path}`)

    // Pipe our initial stream to our new file stream and
    // set that as the new log logstream for future writes
    // if logs max is 0 then the user does not want a log file
    if (this.#logsMax > 0) {
      const initialFile = this.#openLogFile()
      if (initialFile) {
        this.#logStream = this.#logStream.pipe(initialFile)
      }
    }

    // Kickoff cleaning process, even if we aren't writing a logfile.
    // This is async but it will always ignore the current logfile
    // Return the result so it can be awaited in tests
    return this.#cleanLogs()
  }

  log (...args) {
    this.#logHandler(...args)
  }

  get files () {
    return this.#files
  }

  get #isBuffered () {
    return this.#logStream instanceof Minipass
  }

  #endStream (output) {
    if (this.#logStream) {
      this.#logStream.end(output)
      this.#logStream = null
    }
  }

  #logHandler = (level, ...args) => {
    // Ignore pause and resume events since we
    // write everything to the log file
    if (level === 'pause' || level === 'resume') {
      return
    }

    // If the stream is ended then do nothing
    if (!this.#logStream) {
      return
    }

    const logOutput = this.#formatLogItem(level, ...args)

    if (this.#isBuffered) {
      // Cant do anything but buffer the output if we dont
      // have a file stream yet
      this.#logStream.write(logOutput)
      return
    }

    // Open a new log file if we've written too many logs to this one
    if (this.#fileLogCount >= this.#MAX_LOGS_PER_FILE) {
      // Write last chunk to the file and close it
      this.#endStream(logOutput)
      if (this.#files.length >= this.#MAX_FILES_PER_PROCESS) {
        // but if its way too many then we just stop listening
        this.off()
      } else {
        // otherwise we are ready for a new file for the next event
        this.#logStream = this.#openLogFile()
      }
    } else {
      this.#logStream.write(logOutput)
    }
  }

  #formatLogItem (...args) {
    this.#fileLogCount += 1
    return LogFiles.format(this.#totalLogCount++, ...args)
  }

  #getLogFilePath (count = '') {
    return `${this.#path}debug-${count}.log`
  }

  #openLogFile () {
    // Count in filename will be 0 indexed
    const count = this.#files.length

    try {
      // Pad with zeros so that our log files are always sorted properly
      // We never want to write files ending in `-9.log` and `-10.log` because
      // log file cleaning is done by deleting the oldest so in this example
      // `-10.log` would be deleted next
      const f = this.#getLogFilePath(padZero(count, this.#MAX_FILES_PER_PROCESS))
      // Some effort was made to make the async, but we need to write logs
      // during process.on('exit') which has to be synchronous. So in order
      // to never drop log messages, it is easiest to make it sync all the time
      // and this was measured to be about 1.5% slower for 40k lines of output
      const logStream = new fsMiniPass.WriteStreamSync(f, { flags: 'a' })
      if (count > 0) {
        // Reset file log count if we are opening
        // after our first file
        this.#fileLogCount = 0
      }
      this.#files.push(logStream.path)
      return logStream
    } catch (e) {
      // If the user has a readonly logdir then we don't want to
      // warn this on every command so it should be verbose
      log.verbose('logfile', `could not be created: ${e}`)
    }
  }

  async #cleanLogs () {
    // module to clean out the old log files
    // this is a best-effort attempt.  if a rm fails, we just
    // log a message about it and move on.  We do return a
    // Promise that succeeds when we've tried to delete everything,
    // just for the benefit of testing this function properly.

    try {
      const logPath = this.#getLogFilePath()
      const logGlob = join(dirname(logPath), basename(logPath)
        // tell glob to only match digits
        .replace(/\d/g, '[0123456789]')
        // Handle the old (prior to 8.2.0) log file names which did not have a
        // counter suffix
        .replace(/-\.log$/, '*.log')
      )

      // Always ignore the currently written files
      const files = await glob(globify(logGlob), { ignore: this.#files.map(globify), silent: true })
      const toDelete = files.length - this.#logsMax

      if (toDelete <= 0) {
        return
      }

      log.silly('logfile', `start cleaning logs, removing ${toDelete} files`)

      for (const file of files.slice(0, toDelete)) {
        try {
          await fs.rm(file, { force: true })
        } catch (e) {
          log.silly('logfile', 'error removing log file', file, e)
        }
      }
    } catch (e) {
      // Disable cleanup failure warnings when log writing is disabled
      if (this.#logsMax > 0) {
        log.warn('logfile', 'error cleaning log files', e)
      }
    } finally {
      log.silly('logfile', 'done cleaning log files')
    }
  }
}

module.exports = LogFiles
