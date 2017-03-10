function write() {
  try {
    process.stdout.write('Hello, world\n');
  } catch (ex) {
    throw new Error('this should never happen');
  }
  setImmediate(() => {
    write();
  });
}

process.stdout.on('error', (err) => {
  console.error(JSON.stringify(err));
  process.exit(42);
});

write();
