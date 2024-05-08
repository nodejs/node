import { Worker } from 'worker_threads';
import { foo } from './module-named-exports.mjs';

const workerURLFailOnResolve = new URL('./worker-fail-on-resolve.mjs', import.meta.url);
console.log(foo);

// Spawn a worker that will fail to import a dependant module
new Worker(workerURLFailOnResolve);

process.on('exit', (code) => {
	console.log(`process exit code: ${code}`)
});
