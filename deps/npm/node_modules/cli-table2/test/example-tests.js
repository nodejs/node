var printExamples = require('../lib/print-example');
var examples = require('../examples/col-and-row-span-examples');
var usage = require('../examples/basic-usage-examples');

printExamples.runTest('@api Usage',usage);
printExamples.runTest('@api Table (examples)',examples);