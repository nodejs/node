if (require.main === module)
  require('./lib/cli.js')(process)
else
  throw new Error('The programmatic API was removed in npm v8.0.0')
