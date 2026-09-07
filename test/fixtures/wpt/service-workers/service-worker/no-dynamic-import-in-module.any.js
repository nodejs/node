// META: global=serviceworker-module

// This is imported to ensure import('./basic-module-2.js') fails even if
// it has been previously statically imported.
import './resources/basic-module-2.js';

import './resources/no-dynamic-import.js';
