'use strict';

module.exports = ('NODE_UNIQUE_ID' in process.env) ?
                  require('internal/cluster/child') :
                  require('internal/cluster/master');
