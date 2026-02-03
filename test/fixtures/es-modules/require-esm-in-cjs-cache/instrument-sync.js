import * as mod from 'node:module';

mod.registerHooks({
  load(url, context, nextLoad) {
    return nextLoad(url, context);
  },
});
