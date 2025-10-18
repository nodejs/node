process.on('SIGTERM', () => { console.log('__SIGTERM received__'); process.exit(); });
process.on('SIGINT', () => { console.log('__SIGINT received__'); process.exit(); });
process.send('script ready');
setTimeout(() => {}, 100_000);
