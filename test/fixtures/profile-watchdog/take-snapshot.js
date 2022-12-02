const { workerData } = require('worker_threads');
const { MemoryProfileWatchdog } = require('v8');
const fs = require('fs');

const filepath = process.env.filepath || workerData?.filepath;

new MemoryProfileWatchdog({
    maxRss: 1024 * 1024,
    maxUsedHeapSize: 1024 * 1024,
    interval: 1000,
    filename: filepath,
});

const timer = setInterval(() => {
    if (fs.existsSync(filepath)) {
        clearInterval(timer);
    }
}, 1000);
