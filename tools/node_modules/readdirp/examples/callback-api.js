var readdirp = require('..'); 

readdirp({ root: '.', fileFilter: '*.js' }, function (errors, res) {
  if (errors) {
    errors.forEach(function (err) {
      console.error('Error: ', err);
    });
  }
  console.log('all javascript files', res);
});
