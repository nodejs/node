import { root } from './root';
export function minimalSetImpl() {
    // THIS IS NOT a full impl of Set, this is just the minimum
    // bits of functionality we need for this library.
    return class MinimalSet {
        constructor() {
            this._values = [];
        }
        add(value) {
            if (!this.has(value)) {
                this._values.push(value);
            }
        }
        has(value) {
            return this._values.indexOf(value) !== -1;
        }
        get size() {
            return this._values.length;
        }
        clear() {
            this._values.length = 0;
        }
    }
    ;
}
export const Set = root.Set || minimalSetImpl();
//# sourceMappingURL=Set.js.map