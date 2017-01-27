'use strict';

const common = require('../common');

const warning = 'new_feature is an experimental feature. This feature could ' +
                'change at any time.';
common.expectWarning('ExperimentalWarning', warning);
process.emitExperimentalWarning('new_feature');
process.emitExperimentalWarning('new_feature');
