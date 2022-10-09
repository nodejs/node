"use strict";

Object.defineProperty(exports, "t", {
    value: true
});

exports.default = exports.LinkNode = exports.LinkListIterator = void 0;

var _Base = _interopRequireDefault(require("./Base"));

var _ContainerBase = require("../ContainerBase");

function _interopRequireDefault(t) {
    return t && t.t ? t : {
        default: t
    };
}

class LinkNode {
    constructor(t) {
        this.L = undefined;
        this.j = undefined;
        this.O = undefined;
        this.L = t;
    }
}

exports.LinkNode = LinkNode;

class LinkListIterator extends _ContainerBase.ContainerIterator {
    constructor(t, i, s) {
        super(s);
        this.I = t;
        this.S = i;
        if (this.iteratorType === 0) {
            this.pre = function() {
                if (this.I.j === this.S) {
                    throw new RangeError("LinkList iterator access denied!");
                }
                this.I = this.I.j;
                return this;
            };
            this.next = function() {
                if (this.I === this.S) {
                    throw new RangeError("LinkList iterator access denied!");
                }
                this.I = this.I.O;
                return this;
            };
        } else {
            this.pre = function() {
                if (this.I.O === this.S) {
                    throw new RangeError("LinkList iterator access denied!");
                }
                this.I = this.I.O;
                return this;
            };
            this.next = function() {
                if (this.I === this.S) {
                    throw new RangeError("LinkList iterator access denied!");
                }
                this.I = this.I.j;
                return this;
            };
        }
    }
    get pointer() {
        if (this.I === this.S) {
            throw new RangeError("LinkList iterator access denied!");
        }
        return this.I.L;
    }
    set pointer(t) {
        if (this.I === this.S) {
            throw new RangeError("LinkList iterator access denied!");
        }
        this.I.L = t;
    }
    equals(t) {
        return this.I === t.I;
    }
    copy() {
        return new LinkListIterator(this.I, this.S, this.iteratorType);
    }
}

exports.LinkListIterator = LinkListIterator;

class LinkList extends _Base.default {
    constructor(t = []) {
        super();
        this.S = new LinkNode;
        this.V = undefined;
        this.G = undefined;
        t.forEach((t => this.pushBack(t)));
    }
    clear() {
        this.o = 0;
        this.V = this.G = undefined;
        this.S.j = this.S.O = undefined;
    }
    begin() {
        return new LinkListIterator(this.V || this.S, this.S);
    }
    end() {
        return new LinkListIterator(this.S, this.S);
    }
    rBegin() {
        return new LinkListIterator(this.G || this.S, this.S, 1);
    }
    rEnd() {
        return new LinkListIterator(this.S, this.S, 1);
    }
    front() {
        return this.V ? this.V.L : undefined;
    }
    back() {
        return this.G ? this.G.L : undefined;
    }
    forEach(t) {
        if (!this.o) return;
        let i = this.V;
        let s = 0;
        while (i !== this.S) {
            t(i.L, s++);
            i = i.O;
        }
    }
    getElementByPos(t) {
        if (t < 0 || t > this.o - 1) {
            throw new RangeError;
        }
        let i = this.V;
        while (t--) {
            i = i.O;
        }
        return i.L;
    }
    eraseElementByPos(t) {
        if (t < 0 || t > this.o - 1) {
            throw new RangeError;
        }
        if (t === 0) this.popFront(); else if (t === this.o - 1) this.popBack(); else {
            let i = this.V;
            while (t--) {
                i = i.O;
            }
            i = i;
            const s = i.j;
            const e = i.O;
            e.j = s;
            s.O = e;
            this.o -= 1;
        }
    }
    eraseElementByValue(t) {
        while (this.V && this.V.L === t) this.popFront();
        while (this.G && this.G.L === t) this.popBack();
        if (!this.V) return;
        let i = this.V;
        while (i !== this.S) {
            if (i.L === t) {
                const t = i.j;
                const s = i.O;
                s.j = t;
                t.O = s;
                this.o -= 1;
            }
            i = i.O;
        }
    }
    eraseElementByIterator(t) {
        const i = t.I;
        if (i === this.S) {
            throw new RangeError("Invalid iterator");
        }
        t = t.next();
        if (this.V === i) this.popFront(); else if (this.G === i) this.popBack(); else {
            const t = i.j;
            const s = i.O;
            s.j = t;
            t.O = s;
            this.o -= 1;
        }
        return t;
    }
    pushBack(t) {
        this.o += 1;
        const i = new LinkNode(t);
        if (!this.G) {
            this.V = this.G = i;
            this.S.O = this.V;
            this.V.j = this.S;
        } else {
            this.G.O = i;
            i.j = this.G;
            this.G = i;
        }
        this.G.O = this.S;
        this.S.j = this.G;
    }
    popBack() {
        if (!this.G) return;
        this.o -= 1;
        if (this.V === this.G) {
            this.V = this.G = undefined;
            this.S.O = undefined;
        } else {
            this.G = this.G.j;
            this.G.O = this.S;
        }
        this.S.j = this.G;
    }
    setElementByPos(t, i) {
        if (t < 0 || t > this.o - 1) {
            throw new RangeError;
        }
        let s = this.V;
        while (t--) {
            s = s.O;
        }
        s.L = i;
    }
    insert(t, i, s = 1) {
        if (t < 0 || t > this.o) {
            throw new RangeError;
        }
        if (s <= 0) return;
        if (t === 0) {
            while (s--) this.pushFront(i);
        } else if (t === this.o) {
            while (s--) this.pushBack(i);
        } else {
            let e = this.V;
            for (let i = 1; i < t; ++i) {
                e = e.O;
            }
            const h = e.O;
            this.o += s;
            while (s--) {
                e.O = new LinkNode(i);
                e.O.j = e;
                e = e.O;
            }
            e.O = h;
            h.j = e;
        }
    }
    find(t) {
        if (!this.V) return this.end();
        let i = this.V;
        while (i !== this.S) {
            if (i.L === t) {
                return new LinkListIterator(i, this.S);
            }
            i = i.O;
        }
        return this.end();
    }
    reverse() {
        if (this.o <= 1) return;
        let t = this.V;
        let i = this.G;
        let s = 0;
        while (s << 1 < this.o) {
            const e = t.L;
            t.L = i.L;
            i.L = e;
            t = t.O;
            i = i.j;
            s += 1;
        }
    }
    unique() {
        if (this.o <= 1) return;
        let t = this.V;
        while (t !== this.S) {
            let i = t;
            while (i.O && i.L === i.O.L) {
                i = i.O;
                this.o -= 1;
            }
            t.O = i.O;
            t.O.j = t;
            t = t.O;
        }
    }
    sort(t) {
        if (this.o <= 1) return;
        const i = [];
        this.forEach((t => i.push(t)));
        i.sort(t);
        let s = this.V;
        i.forEach((t => {
            s.L = t;
            s = s.O;
        }));
    }
    pushFront(t) {
        this.o += 1;
        const i = new LinkNode(t);
        if (!this.V) {
            this.V = this.G = i;
            this.G.O = this.S;
            this.S.j = this.G;
        } else {
            i.O = this.V;
            this.V.j = i;
            this.V = i;
        }
        this.S.O = this.V;
        this.V.j = this.S;
    }
    popFront() {
        if (!this.V) return;
        this.o -= 1;
        if (this.V === this.G) {
            this.V = this.G = undefined;
            this.S.j = this.G;
        } else {
            this.V = this.V.O;
            this.V.j = this.S;
        }
        this.S.O = this.V;
    }
    merge(t) {
        if (!this.V) {
            t.forEach((t => this.pushBack(t)));
            return;
        }
        let i = this.V;
        t.forEach((t => {
            while (i && i !== this.S && i.L <= t) {
                i = i.O;
            }
            if (i === this.S) {
                this.pushBack(t);
                i = this.G;
            } else if (i === this.V) {
                this.pushFront(t);
                i = this.V;
            } else {
                this.o += 1;
                const s = i.j;
                s.O = new LinkNode(t);
                s.O.j = s;
                s.O.O = i;
                i.j = s.O;
            }
        }));
    }
    [Symbol.iterator]() {
        return function*() {
            if (!this.V) return;
            let t = this.V;
            while (t !== this.S) {
                yield t.L;
                t = t.O;
            }
        }.bind(this)();
    }
}

var _default = LinkList;

exports.default = _default;