// ensure we're a CJS
require('path');


function goodError(err) {
    return err instanceof Error && err.message === 'hello'
}

try {
    require('./runtime-error-esm.mjs')
    process.exit(1)
} catch (err) {
    if (goodError(err)) {
        console.log('CJS_OK')
    } else {
        process.exit(1)
    }
}

import('./runtime-error-esm.mjs').then(() => {
    process.exit(1)
}).catch((err) => {
    if (goodError(err)) {
        console.log('ESM_OK')
    } else {
        process.exit(1)
    }
})
