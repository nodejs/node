import { registerHooks } from 'node:module';

registerHooks({
  load(url, context, nextLoad) {
    if (url.endsWith('app.js')) {
      return {
        shortCircuit: true,
        format: 'module',
        source: 'console.log("customized by sync hook")',
      };
    }
    return nextLoad(url, context);
  },
});
