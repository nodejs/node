import { register } from 'node:module';
import { Worker, workerData, threadId } from 'node:worker_threads';
import { fileURL } from '../../common/fixtures.mjs';

if (workerData.port) {
  workerData.port.postMessage('MESSAGE');
}

if (workerData.count === 2) {
  register(fileURL('es-module-loaders/hooks-log-verbose-loader.mjs'), import.meta.url, { data : {  label: 'first' } });
}

if (workerData.count === 3) {
  register(fileURL('es-module-loaders/hooks-log-verbose-loader.mjs'), import.meta.url, { data : { label: 'second' } });
}

if (workerData.count === 1) {
  const { port1, port2 } = new MessageChannel();

  port1.on('message', message => {
    process._rawDebug(JSON.stringify({ operation: 'message', message }));
  }).unref();

  new Worker(
    fileURL('es-modules/test-esm-separate-chains.mjs'), 
    { workerData: { count: workerData.count + 1, port: port2 },
    transferList: [port2],
  });  
} else if (workerData.count < 3) {
  new Worker(fileURL('es-modules/test-esm-separate-chains.mjs'), { workerData: { count: workerData.count + 1 } });
}

await import(fileURL('es-modules/test-esm-separate-chains-execute.mjs'));