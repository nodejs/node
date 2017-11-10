var readdirp =  require('..')
  , path = require('path');

readdirp({ root: path.join(__dirname), fileFilter: '*.js' })
  .on('warn', function (err) { 
    console.error('something went wrong when processing an entry', err); 
  })
  .on('error', function (err) { 
    console.error('something went fatally wrong and the stream was aborted', err); 
  })
  .on('data', function (entry) { 
    console.log('%s is ready for processing', entry.path);
    // process entry here
  });

