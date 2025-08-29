'use strict';
Object.defineProperty(exports, "__esModule", { value: true });
exports.simpleErrorStack = simpleErrorStack;
// Compile with `tsc --inlineSourceMap benchmark/fixtures/simple-error-stack.ts`.
var lorem = 'Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.';
function simpleErrorStack() {
    [1].map(function () {
        try {
            lorem.BANG();
        }
        catch (e) {
            return e.stack;
        }
    });
}
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJmaWxlIjoic2ltcGxlLWVycm9yLXN0YWNrLmpzIiwic291cmNlUm9vdCI6IiIsInNvdXJjZXMiOlsic2ltcGxlLWVycm9yLXN0YWNrLnRzIl0sIm5hbWVzIjpbXSwibWFwcGluZ3MiOiJBQUFBLFlBQVksQ0FBQzs7QUFpQlgsNENBQWdCO0FBZmxCLGlGQUFpRjtBQUVqRixJQUFNLEtBQUssR0FBRywrYkFBK2IsQ0FBQztBQUU5YyxTQUFTLGdCQUFnQjtJQUN2QixDQUFDLENBQUMsQ0FBQyxDQUFDLEdBQUcsQ0FBQztRQUNOLElBQUksQ0FBQztZQUNGLEtBQWEsQ0FBQyxJQUFJLEVBQUUsQ0FBQztRQUN4QixDQUFDO1FBQUMsT0FBTyxDQUFDLEVBQUUsQ0FBQztZQUNYLE9BQU8sQ0FBQyxDQUFDLEtBQUssQ0FBQztRQUNqQixDQUFDO0lBQ0gsQ0FBQyxDQUFDLENBQUE7QUFDSixDQUFDIn0=