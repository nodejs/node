(function() {
  var fs = require('fs');
  if (fs.readFile) {
    require('util').print('fs loaded successfully');
  }
})();

