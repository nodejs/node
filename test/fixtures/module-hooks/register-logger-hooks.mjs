import { registerHooks } from 'node:module';
import { writeSync } from 'node:fs';

registerHooks({
  resolve(specifier, context, nextResolve) {
    writeSync(1, `resolve ${specifier} from ${context.parentURL}\n`);
    return nextResolve(specifier, context);
  },
  load(url, context, nextLoad) {
    writeSync(1, `load ${url}\n`);
    return nextLoad(url, context);
  },
});
