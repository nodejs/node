if (process.argv[2] === 'handle-fatal-exception')
  process._fatalException = process.exit.bind(null, 42);

require('vm').runInDebugContext('*');
