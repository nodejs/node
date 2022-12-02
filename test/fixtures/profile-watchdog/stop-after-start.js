const { MemoryProfileWatchdog } = require('v8');
const watchdog = new MemoryProfileWatchdog({
    interval: 1000,
    maxRss: 1024,
    filename: 'dummy.heapsnapshot',
});
watchdog.stop();
