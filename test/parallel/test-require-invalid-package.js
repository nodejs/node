'use strict';

const common = require('../common');

// Should be an invalid package path.
common.expectsError(() => require('package.json'),
                    { code: 'MODULE_NOT_FOUND' }
);
