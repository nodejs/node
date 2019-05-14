import { Observable } from '../Observable';
import { isArray } from '../util/isArray';
import { EMPTY } from './empty';
import { subscribeToResult } from '../util/subscribeToResult';
import { OuterSubscriber } from '../OuterSubscriber';
import { map } from '../operators/map';
export function forkJoin(...sources) {
    let resultSelector;
    if (typeof sources[sources.length - 1] === 'function') {
        resultSelector = sources.pop();
    }
    if (sources.length === 1 && isArray(sources[0])) {
        sources = sources[0];
    }
    if (sources.length === 0) {
        return EMPTY;
    }
    if (resultSelector) {
        return forkJoin(sources).pipe(map(args => resultSelector(...args)));
    }
    return new Observable(subscriber => {
        return new ForkJoinSubscriber(subscriber, sources);
    });
}
class ForkJoinSubscriber extends OuterSubscriber {
    constructor(destination, sources) {
        super(destination);
        this.sources = sources;
        this.completed = 0;
        this.haveValues = 0;
        const len = sources.length;
        this.values = new Array(len);
        for (let i = 0; i < len; i++) {
            const source = sources[i];
            const innerSubscription = subscribeToResult(this, source, null, i);
            if (innerSubscription) {
                this.add(innerSubscription);
            }
        }
    }
    notifyNext(outerValue, innerValue, outerIndex, innerIndex, innerSub) {
        this.values[outerIndex] = innerValue;
        if (!innerSub._hasValue) {
            innerSub._hasValue = true;
            this.haveValues++;
        }
    }
    notifyComplete(innerSub) {
        const { destination, haveValues, values } = this;
        const len = values.length;
        if (!innerSub._hasValue) {
            destination.complete();
            return;
        }
        this.completed++;
        if (this.completed !== len) {
            return;
        }
        if (haveValues === len) {
            destination.next(values);
        }
        destination.complete();
    }
}
//# sourceMappingURL=forkJoin.js.map