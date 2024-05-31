import { workerData , threadId} from 'node:worker_threads';

console.log(JSON.stringify({ operation: 'execute', threadId, workerData }));