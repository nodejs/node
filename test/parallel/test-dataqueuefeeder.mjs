// Flags: --expose-internals --no-warnings
import {
  rejects,
} from 'node:assert';

const { internalBinding } = (await import('internal/test/binding')).default;

const {
  DataQueueFeeder,
} = internalBinding('dataqueuefeeder');


const feeder = new DataQueueFeeder();
feeder.addFakePull();
await feeder.ready();
let lastprom = feeder.submit(new Uint8Array([1, 2, 3]), false);
feeder.addFakePull();
await lastprom;
lastprom = feeder.submit(new Uint16Array([1, 2, 3]), false);
feeder.addFakePull();
await lastprom;
lastprom = feeder.submit(new Uint32Array([1, 2, 3]), false);
feeder.addFakePull();
await lastprom;
lastprom = feeder.submit(new Uint8Array([1, 2, 3]).buffer, false);
feeder.addFakePull();
await lastprom;
await rejects(async () => feeder.submit({}, false), {
  code: 'ERR_INVALID_ARG_TYPE',
});
await rejects(async () => feeder.submit([], false), {
  code: 'ERR_INVALID_ARG_TYPE',
});
await feeder.submit(undefined, true);
feeder.error(new Error('Can we send an error'));
