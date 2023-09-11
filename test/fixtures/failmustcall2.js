const common = require('../common');
const f = common.mustCallAtLeast(() => {}, 2);
f();
