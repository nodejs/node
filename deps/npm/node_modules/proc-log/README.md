# proc-log

Emits 'log' events on the process object which a log output listener can
consume and print to the terminal.

This is used by various modules within the npm CLI stack in order to send
log events that [`npmlog`](http://npm.im/npmlog) can consume and print.

## API

* `log.error(...args)` calls `process.emit('log', 'error', ...args)`
  The highest log level.  For printing extremely serious errors that
  indicate something went wrong.
* `log.warn(...args)` calls `process.emit('log', 'warn', ...args)`
  A fairly high log level.  Things that the user needs to be aware of, but
  which won't necessarily cause improper functioning of the system.
* `log.notice(...args)` calls `process.emit('log', 'notice', ...args)`
  Notices which are important, but not necessarily dangerous or a cause for
  excess concern.
* `log.info(...args)` calls `process.emit('log', 'info', ...args)`
  Informative messages that may benefit the user, but aren't particularly
  important.
* `log.verbose(...args)` calls `process.emit('log', 'verbose', ...args)`
  Noisy output that is more detail that most users will care about.
* `log.silly(...args)` calls `process.emit('log', 'silly', ...args)`
  Extremely noisy excessive logging messages that are typically only useful
  for debugging.
* `log.http(...args)` calls `process.emit('log', 'http', ...args)`
  Information about HTTP requests made and/or completed.
* `log.pause(...args)` calls `process.emit('log', 'pause')`  Used to tell
  the consumer to stop printing messages.
* `log.resume(...args)` calls `process.emit('log', 'resume', ...args)`
  Used to tell the consumer that it is ok to print messages again.
