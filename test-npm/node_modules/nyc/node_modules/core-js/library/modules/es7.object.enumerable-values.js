// https://github.com/leobalter/object-enumerables
var $export  = require('./_export')
  , toObject = require('./_to-object');

$export($export.S, 'Object', {
  enumerableValues: function enumerableValues(O){
    var T          = toObject(O)
      , properties = [];
    for(var key in T)properties.push(T[key]);
    return properties;
  }
});