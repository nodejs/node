'use strict';
exports.__esModule = true;
exports.simpleErrorStack = void 0;
// Compile with `tsc --inlineSourceMap benchmark/fixtures/simple-error-stack.ts`.
var lorem = 'Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.';
function simpleErrorStack() {
    try {
        lorem.BANG();
    }
    catch (e) {
        return e.stack;
    }
}
exports.simpleErrorStack = simpleErrorStack;
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJmaWxlIjoic2ltcGxlLWVycm9yLXN0YWNrLmpzIiwic291cmNlUm9vdCI6IiIsInNvdXJjZXMiOlsic2ltcGxlLWVycm9yLXN0YWNrLnRzIl0sIm5hbWVzIjpbXSwibWFwcGluZ3MiOiJBQUFBLFlBQVksQ0FBQzs7O0FBRWIsaUZBQWlGO0FBRWpGLElBQU0sS0FBSyxHQUFHLCtiQUErYixDQUFDO0FBRTljLFNBQVMsZ0JBQWdCO0lBQ3ZCLElBQUk7UUFDRCxLQUFhLENBQUMsSUFBSSxFQUFFLENBQUM7S0FDdkI7SUFBQyxPQUFPLENBQUMsRUFBRTtRQUNWLE9BQU8sQ0FBQyxDQUFDLEtBQUssQ0FBQztLQUNoQjtBQUNILENBQUM7QUFHQyw0Q0FBZ0IifQ==
