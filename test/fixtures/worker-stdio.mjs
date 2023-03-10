import {
    Worker,
    isMainThread,
} from 'worker_threads';
  
if (isMainThread) {
    new Worker(new URL(import.meta.url));
    while(true); // never-ending loop
} else {
    // This output will be blocked by the for loop in the main thread.
    console.log('foo');
    console.error('bar');
}
