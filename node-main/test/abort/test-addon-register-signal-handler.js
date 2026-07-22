'use strict';
require('../common');

// This is a sibling test to test/addons/register-signal-handler/

process.env.ALLOW_CRASHES = true;
require('../addons/register-signal-handler/test');
