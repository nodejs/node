// https://github.com/leobalter/object-enumerables
var $export  = require('./_export')
  , toObject = require('./_to-object');

$export($export.S, 'Object', {
  enumerableKeys: function enumerableKeys(O){
    var T          = toObject(O)
      , properties = [];
    for(var key in T)properties.push(key);
    return properties;
  }
});