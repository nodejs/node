process.on('SIGTERM', () => {
  console.log(`__SIGTERM received__ ${process.pid}`);
  process.exit();
});
process.on('SIGINT', () => {
  console.log(`__SIGINT received__ ${process.pid}`);
  process.exit();
});
process.send(`script ready ${process.pid}`);
const timeout = 100_000;
setTimeout(() => {
  process._rawDebug(`[CHILD] Timeout ${timeout} fired`);
}, timeout);
