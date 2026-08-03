// Flags: --permission --allow-net --allow-fs-read=*
'use strict';

const common = require('../common');

common.expectWarning('ExperimentalWarning',
                     'The flag --allow-net is under experimental phase.');
