'use strict';

const common = require('../common');
const fs = require('fs');

{
  const s = fs.createReadStream(__filename);

  s.close(common.mustCall());
  s.close(common.mustCall());
}

{
  const s = fs.createReadStream(__filename);

  // this is a private API, but it is worth testing. close calls this
  s.destroy(null, common.mustCall());
  s.destroy(null, common.mustCall());
}
