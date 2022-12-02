const { MemoryProfileWatchdog } = require('v8');
const watchdog = new MemoryProfileWatchdog({
    interval: 3000,
    maxRss: 1024,
    filename: 'dummy.heapsnapshot'
});
setTimeout(() => {
    watchdog.stop();
}, 1000);
