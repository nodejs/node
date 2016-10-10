#!/usr/bin/env node
console.log('%j', process.execArgv)
console.log('%j', process.argv.slice(2))

// Keep the event loop alive long enough to receive signals.
setTimeout(function() {}, 100)
