function tmp() {}
process.on('SIGINT', tmp);
process.removeListener('SIGINT', tmp);
setInterval(function() {
  process.stdout.write('keep alive\n');
}, 1000);
process.stdout.write('start\n');
