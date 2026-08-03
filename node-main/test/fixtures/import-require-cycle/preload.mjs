import * as mod from "module";

mod.registerHooks({
  load(url, context, nextLoad) {
    return nextLoad(url, context);
  },
});
