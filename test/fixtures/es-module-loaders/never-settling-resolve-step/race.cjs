'use strict';

Promise.race([
    import('never-settle-resolve'),
    import('never-settle-load'),
    import('node:process'),
]).then(result => console.log(result.default === process));
