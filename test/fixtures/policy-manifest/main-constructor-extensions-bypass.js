const m = new require.main.constructor();
require.extensions['.js'](m, './invalid-module')
