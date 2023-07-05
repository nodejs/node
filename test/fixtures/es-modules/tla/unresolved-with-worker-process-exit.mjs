import { Worker, isMainThread } from 'worker_threads';

if (isMainThread) {
  new Worker(new URL(import.meta.url));
  await new Promise(() => {});
} else {
  process.exit();
}
