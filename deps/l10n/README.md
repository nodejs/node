Localization bundle for Node.

This is pretty straightforward... if ICU is present and --with-intl-full-icu,
then ICU's resource bundle mechanism is used, the resources are compiled
statically into the library, which can then be used within Node. If ICU is not
present, a simple printf fallback is used.

```
./configure --with-intl=full-icu
make
```
When the --with-intl=full-icu switch is on, the resources are compiled into a
static library that is statically linked into Node. The next step will be to
make it so that additional bundles can be specified at runtime.

Resource bundles are located in the resources directory. Standard ICU bundle
format but keep it simple, we currently only support top level resources.

Within the C/C++ code, use the macros:

```cc
#include "node_l10n.h"

L10N_PRINT("TEST", "This is the fallback");
L10N_PRINTF("TEST2", "Testing %s %d", "a", 1);
```

In the JS code, use the internal/l10n.js
```javascript
const l10n = require('internal/l10n');
console.log(l10n("TEST", "This is the fallback"));
console.log(l10n("TEST2", "Fallback %s %d", "a", 1));
```

Use the `--icu-data-dir` switch to specify a location containing alternative
node.dat files containing alternative translations. Note that this is the
same switch used to specify alternative ICU common data files.

One approach that ought to work here is publishing translation node.dat files
to npm. Then, the developer does a `npm install node_dat_de` (for instance)
to grab the appropriate translations. Then, then can start node like:

`node --icu-data-dir=node_modules/node_dat_de` and have things just work.
