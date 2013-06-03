
var child_process = require('child_process');

// Re-export the child_process module.
module.exports = child_process;

// Only node versions up to v0.7.6 need this hook.
if (!/^v0\.([0-6]\.|7\.[0-6](\D|$))/.test(process.version))
  return;

// Do not add the hook if already hooked.
if (child_process.hasOwnProperty('_exit_hook'))
  return;

// Version the hook in case there is ever the need to release a 0.2.0.
child_process._exit_hook = 1;


function hook(name) {
  var orig = child_process[name];

  // Older node versions may not have all functions, e.g. fork().
  if (!orig)
    return;

  // Store the unhooked version.
  child_process['_original_' + name] = orig;

  // Do the actual hooking.
  child_process[name] = function() {
    var child = orig.apply(this, arguments);

    child.once('exit', function(code, signal) {
      process.nextTick(function() {
        child.emit('close', code, signal);
      });
    });

    return child;
  }
}

hook('spawn');
hook('fork');
hook('execFile');

// Don't hook 'exec': it calls `exports.execFile` internally, so hooking it
// would trigger the close event twice.
