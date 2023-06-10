'use strict';

/**
* Escape quoted values in a string template literal. On Windows, this function
* does not escape anything (which is fine for paths, as `"` is not a valid char
* in a path on Windows), so you should use it only to escape paths – or other
* values on tests which are skipped on Windows.
* @returns {[string, object | undefined]} An array that can be passed as
*                                         arguments to `exec` or `execSync`.
*/
module.exports = function escapePOSIXShell(cmdParts, ...args) {
 if (common.isWindows) {
   // On Windows, paths cannot contain `"`, so we can return the string unchanged.
   return [String.raw({ raw: cmdParts }, ...args)];
 }
 // On POSIX shells, we can pass values via the env, as there's a standard way for referencing a variable.
 const env = { ...process.env };
 let cmd = cmdParts[0];
 for (let i = 0; i < args.length; i++) {
   const envVarName = `ESCAPED_${i}`;
   env[envVarName] = args[i];
   cmd += '${' + envVarName + '}' + cmdParts[i + 1];
 }

 return [cmd, { env }];
};
