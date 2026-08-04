import { parentPort, workerData } from 'node:worker_threads';

const { url } = workerData;
try {
  const res = await fetch(url);
  const text = await res.text();
  parentPort.postMessage({ ok: true, status: res.status, text });
} catch (err) {
  parentPort.postMessage({
    ok: false,
    code: err?.cause?.code,
    message: err.message,
  });
}
