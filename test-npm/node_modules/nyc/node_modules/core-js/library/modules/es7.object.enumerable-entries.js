// https://github.com/leobalter/object-enumerables
var $export  = require('./_export')
  , toObject = require('./_to-object');

$export($export.S, 'Object', {
  enumerableEntries: function enumerableEntries(O){
    var T          = toObject(O)
      , properties = [];
    for(var key in T)properties.push([key, T[key]]);
    return properties;
  }
});