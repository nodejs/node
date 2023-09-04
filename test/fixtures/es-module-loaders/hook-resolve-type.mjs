import * as fixtures from '../../common/fixtures.mjs';
import { register } from 'node:module';
import { MessageChannel } from 'node:worker_threads';

let importedESM = 0;
let importedCJS = 0;
export function getModuleTypeStats() {
  return { importedESM, importedCJS };
};

const { port1, port2 } = new MessageChannel();

register(fixtures.fileURL('es-module-loaders/hook-resolve-type-loader.mjs'), {
  data: { port: port2 },
  transferList: [port2],
});

port1.on('message', ({ type }) => {
  switch (type) {
    case 'module':
      importedESM++;
      break;
    case 'commonjs':
      importedCJS++;
      break;
  }
});

port1.unref();
port2.unref();
