const testCases = [
    {
        scriptURL: '/workers/modules/resources/static-import-worker.js',
        expectation: ['export-on-load-script.js'],
        description: 'Static import.'
    },
    {
        scriptURL: '/workers/modules/resources/static-import-remote-origin-script-worker.sub.js',
        expectation: ['export-on-load-script.js'],
        description: 'Static import (cross-origin).'
    },
    {
        scriptURL: '/workers/modules/resources/static-import-redirect-worker.js',
        expectation: ['export-on-load-script.js'],
        description: 'Static import (redirect).'
    },
    {
        scriptURL: '/workers/modules/resources/nested-static-import-worker.js',
        expectation: [
            'export-on-static-import-script.js',
            'export-on-load-script.js'
        ],
        description: 'Nested static import.'
    },
    {
        scriptURL: '/workers/modules/resources/static-import-and-then-dynamic-import-worker.js',
        expectation: [
            'export-on-dynamic-import-script.js',
            'export-on-load-script.js'
        ],
        description: 'Static import and then dynamic import.'
    },
    {
        scriptURL: '/workers/modules/resources/dynamic-import-worker.js',
        expectation: ['export-on-load-script.js'],
        description: 'Dynamic import.'
    },
    {
        scriptURL: '/workers/modules/resources/nested-dynamic-import-worker.js',
        expectation: [
            'export-on-dynamic-import-script.js',
            'export-on-load-script.js'
        ],
        description: 'Nested dynamic import.'
    },
    {
        scriptURL: '/workers/modules/resources/dynamic-import-and-then-static-import-worker.js',
        expectation: [
            'export-on-static-import-script.js',
            'export-on-load-script.js'
        ],
        description: 'Dynamic import and then static import.'
    },
    {
        scriptURL: '/workers/modules/resources/eval-dynamic-import-worker.js',
        expectation: ['export-on-load-script.js'],
        description: 'eval(import()).'
    }
];
