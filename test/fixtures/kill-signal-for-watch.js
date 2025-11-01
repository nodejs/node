process.on('SIGTERM', () => {
  console.log(`__SIGTERM received__ ${process.pid}`);
  process.exit();
});
process.on('SIGINT', () => {
  console.log(`__SIGINT received__ ${process.pid}`);
  process.exit();
});
process.send(`script ready ${process.pid}`);
setTimeout(() => {}, 100_000);
