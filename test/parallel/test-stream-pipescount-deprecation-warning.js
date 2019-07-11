'use strict';

const common = require('../common');
const { Readable } = require('stream');

const warning = '_readableState.pipesCount is deprecated. ' +
                'Use _readableState.pipes.length instead.';

common.expectWarning('DeprecationWarning', warning, 'DEP0133');

const readable = new Readable();
readable._readableState.pipesCount;
