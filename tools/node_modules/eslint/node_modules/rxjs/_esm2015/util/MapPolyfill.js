export class MapPolyfill {
    constructor() {
        this.size = 0;
        this._values = [];
        this._keys = [];
    }
    get(key) {
        const i = this._keys.indexOf(key);
        return i === -1 ? undefined : this._values[i];
    }
    set(key, value) {
        const i = this._keys.indexOf(key);
        if (i === -1) {
            this._keys.push(key);
            this._values.push(value);
            this.size++;
        }
        else {
            this._values[i] = value;
        }
        return this;
    }
    delete(key) {
        const i = this._keys.indexOf(key);
        if (i === -1) {
            return false;
        }
        this._values.splice(i, 1);
        this._keys.splice(i, 1);
        this.size--;
        return true;
    }
    clear() {
        this._keys.length = 0;
        this._values.length = 0;
        this.size = 0;
    }
    forEach(cb, thisArg) {
        for (let i = 0; i < this.size; i++) {
            cb.call(thisArg, this._values[i], this._keys[i]);
        }
    }
}
//# sourceMappingURL=MapPolyfill.js.map