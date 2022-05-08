(function (global, factory) {
    typeof exports === 'object' && typeof module !== 'undefined' ? factory(exports) :
    typeof define === 'function' && define.amd ? define(['exports'], factory) :
    (global = typeof globalThis !== 'undefined' ? globalThis : global || self, factory(global.setArray = {}));
})(this, (function (exports) { 'use strict';

    /**
     * Gets the index associated with `key` in the backing array, if it is already present.
     */
    exports.get = void 0;
    /**
     * Puts `key` into the backing array, if it is not already present. Returns
     * the index of the `key` in the backing array.
     */
    exports.put = void 0;
    /**
     * Pops the last added item out of the SetArray.
     */
    exports.pop = void 0;
    /**
     * SetArray acts like a `Set` (allowing only one occurrence of a string `key`), but provides the
     * index of the `key` in the backing array.
     *
     * This is designed to allow synchronizing a second array with the contents of the backing array,
     * like how in a sourcemap `sourcesContent[i]` is the source content associated with `source[i]`,
     * and there are never duplicates.
     */
    class SetArray {
        constructor() {
            this._indexes = { __proto__: null };
            this.array = [];
        }
    }
    (() => {
        exports.get = (strarr, key) => strarr._indexes[key];
        exports.put = (strarr, key) => {
            // The key may or may not be present. If it is present, it's a number.
            const index = exports.get(strarr, key);
            if (index !== undefined)
                return index;
            const { array, _indexes: indexes } = strarr;
            return (indexes[key] = array.push(key) - 1);
        };
        exports.pop = (strarr) => {
            const { array, _indexes: indexes } = strarr;
            if (array.length === 0)
                return;
            const last = array.pop();
            indexes[last] = undefined;
        };
    })();

    exports.SetArray = SetArray;

    Object.defineProperty(exports, '__esModule', { value: true });

}));
//# sourceMappingURL=set-array.umd.js.map
