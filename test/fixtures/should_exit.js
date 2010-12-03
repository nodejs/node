function tmp() {}
process.addListener('SIGINT', tmp);
process.removeListener('SIGINT', tmp);
setInterval(function() {
  process.stdout.write('keep alive\n');
}, 1000);
