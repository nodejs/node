"use strict";
// Usage: `node generate-test-wrappers.js js-filename1.js [js-filename2.js ...]` will generate:
// - js-filename1.html
// - js-filename1.sharedworker.html
// - js-filename1.dedicatedworker.html
// - js-filename1.serviceworker.https.html
// (for each passed filename)
//
// It will turn any importScripts inside the .js file into <script>s in the browser context wrapper.
//
// This could become obsolete if https://github.com/web-platform-tests/wpt/issues/4210 gets fixed,
// allowing .any.js to work with all four contexts.

const fs = require("fs");
const path = require("path");

for (const arg of process.argv.slice(2)) {
    generateWrapper(arg);
}

function generateWrapper(jsFilename) {
    const importedScriptFilenames = findImportedScriptFilenames(jsFilename);
    const importedScriptTags = importedScriptFilenames
        .map(filename => `<script src="${filename}"></script>`)
        .join('\n');

    const basename = path.basename(jsFilename);
    const noExtension = path.basename(jsFilename, '.js');

    const outputs = {
        '.html': `<!DOCTYPE html>
<meta charset="utf-8">
<title>${basename} browser context wrapper file</title>

<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>

${importedScriptTags}

<script src="${basename}"></script>
`,
        '.dedicatedworker.html': `<!DOCTYPE html>
<meta charset="utf-8">
<title>${basename} dedicated worker wrapper file</title>

<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>

<script>
'use strict';
fetch_tests_from_worker(new Worker('${basename}'));
</script>
`,
        '.sharedworker.html': `<!DOCTYPE html>
<meta charset="utf-8">
<title>${basename} shared worker wrapper file</title>

<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>

<script>
'use strict';
fetch_tests_from_worker(new SharedWorker('${basename}'));
</script>
`,
        '.serviceworker.https.html': `<!DOCTYPE html>
<meta charset="utf-8">
<title>${basename} service worker wrapper file</title>

<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
<script src="/service-workers/service-worker/resources/test-helpers.sub.js"></script>

<script>
'use strict';
service_worker_test('${basename}', 'Service worker test setup');
</script>
`
    };

    for (const [key, value] of Object.entries(outputs)) {
        const destFilename = path.resolve(path.dirname(jsFilename), `${noExtension}${key}`);
        fs.writeFileSync(destFilename, value, { encoding: 'utf-8' });
    }
}

function findImportedScriptFilenames(inputFilename) {
    const scriptContents = fs.readFileSync(inputFilename, { encoding: 'utf-8' });

    const regExp = /self\.importScripts\('([^']+)'\);/g;

    let result = [];
    let match;
    while (match = regExp.exec(scriptContents)) {
        result.push(match[1]);
    }

    return result.filter(x => x !== '/resources/testharness.js');
}
