var $export = require('./_export')
  , define  = require('./_object-define')
  , create  = require('./_object-create');

$export($export.S + $export.F, 'Object', {
  make: function(proto, mixin){
    return define(create(proto), mixin);
  }
});