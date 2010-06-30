require('../common')

process.addListener('uncaughtException', function (err) {
  puts('Caught exception: ' + err);
});

setTimeout(function () {
  puts('This will still run.');
}, 500);

// Intentionally cause an exception, but don't catch it.
nonexistentFunc();
puts('This will not run.');
