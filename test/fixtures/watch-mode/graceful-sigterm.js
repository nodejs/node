
setInterval(() => {}, 1000);
console.log('running');

process.on('SIGTERM', () => {
  setTimeout(() => {
    console.log('exiting gracefully');
    process.exit(0);
  }, 1000);
});

process.on('SIGINT', () => {
  setTimeout(() => {
    console.log('exiting gracefully');
    process.exit(0);
  }, 1000);
});
