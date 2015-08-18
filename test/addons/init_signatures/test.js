'use strict';
var assert = require('assert')
  , path   = require('path')
  , i, name, addon
  , signatures = [ ['exports']
                 , ['exports', 'module']
                 , ['exports', 'context']
                 , ['exports', 'private']
                 , ['exports', 'module', 'private']
                 , ['exports', 'module', 'context']
                 , ['exports', 'module', 'context', 'private']
                 ];
for (i in signatures) {
  name  = 'init_' + signatures[i].join('_');
  addon = require('./build/Release/' + name);
  assert.ok(addon.initialized);
}
