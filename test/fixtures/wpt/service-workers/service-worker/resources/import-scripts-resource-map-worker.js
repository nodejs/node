importScripts('/resources/testharness.js');

let version = null;
importScripts('import-scripts-version.py');
// Once imported, the stored script should be loaded for subsequent importScripts.
const expected_version = version;

version = null;
importScripts('import-scripts-version.py');
assert_equals(expected_version, version, 'second import');

version = null;
importScripts('import-scripts-version.py', 'import-scripts-version.py',
    'import-scripts-version.py');
assert_equals(expected_version, version, 'multiple imports');
