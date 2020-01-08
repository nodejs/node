'use strict';

process.on(process.uncaughtExceptionMonitor, (err) => {
	console.log(`Monitored: ${err.message}`);
});

throw new Error('Shall exit');
