var builtins = require('./builtins');

builtins.forEach(function(name){
  require(name);
});
