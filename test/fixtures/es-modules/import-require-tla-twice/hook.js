const { registerHooks } = require('module');
registerHooks({
  load(url, context, nextLoad) {
    return nextLoad(url, context);
  }
});
