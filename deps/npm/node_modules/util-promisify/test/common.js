const mustCallChecks = [];

function runCallChecks(exitCode) {
  if (exitCode !== 0) return;

  const failed = mustCallChecks.filter(function(context) {
    return context.actual !== context.expected;
  });

  failed.forEach(function(context) {
    console.log('Mismatched %s function calls. Expected %d, actual %d.',
                context.name,
                context.expected,
                context.actual);
    console.log(context.stack.split('\n').slice(2).join('\n'));
  });

  if (failed.length) process.exit(1);
}

exports.mustCall = function(fn, expected) {
  if (typeof fn === 'number') {
    expected = fn;
    fn = noop;
  } else if (fn === undefined) {
    fn = noop;
  }

  if (expected === undefined)
    expected = 1;
  else if (typeof expected !== 'number')
    throw new TypeError(`Invalid expected value: ${expected}`);

  const context = {
    expected: expected,
    actual: 0,
    stack: (new Error()).stack,
    name: fn.name || '<anonymous>'
  };

  // add the exit listener only once to avoid listener leak warnings
  if (mustCallChecks.length === 0) process.on('exit', runCallChecks);

  mustCallChecks.push(context);

  return function() {
    context.actual++;
    return fn.apply(this, arguments);
  };
};

// Crash the process on unhandled rejections.
exports.crashOnUnhandledRejection = function() {
  process.on('unhandledRejection',
             (err) => process.nextTick(() => { throw err; }));
};
