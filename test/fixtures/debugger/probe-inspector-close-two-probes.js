'use strict';

const fs = require('fs');
const inspector = require('inspector');

fs.writeSync(2, 'probe-inspector-close-marker\n');

const closeInspector = () => { inspector.close(); };

const firstProbeLine = 1;
const secondProbeLine = 2;

setInterval(() => {}, 1000);
