import { parentPort } from 'node:worker_threads';
import './terminate-worker-tla.mjs';

parentPort.postMessage('running');
const end = Date.now() + 5000;
while (Date.now() < end);
