function write() {
  try {
    process.stdout.write('Hello, world\n');
  } catch (ex) {
    throw new Error('this should never happen');
  }
  setImmediate(function() {
    write();
  });
}

process.stdout.on('error', function(er) {
  console.error(JSON.stringify(er));
  process.exit(42);
});

write();
