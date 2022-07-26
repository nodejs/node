const { allocateUntilCrash } = require('../common/allocate-and-check-limits');
const resourceLimits = JSON.parse(process.argv[2]);
allocateUntilCrash(resourceLimits);
