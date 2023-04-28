'use strict';

// The path in "main" in "package.json" does not exist here, but it does in
// the copy in node_modules. This is being tested because bluebird tests depend
// on this behavior and it was accidentally broken by a seemingly unrelated
// commit on the main branch.

require('package-main-enoent');
