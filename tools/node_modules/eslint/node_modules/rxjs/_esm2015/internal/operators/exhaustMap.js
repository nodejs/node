import { OuterSubscriber } from '../OuterSubscriber';
import { InnerSubscriber } from '../InnerSubscriber';
import { subscribeToResult } from '../util/subscribeToResult';
import { map } from './map';
import { from } from '../observable/from';
export function exhaustMap(project, resultSelector) {
    if (resultSelector) {
        return (source) => source.pipe(exhaustMap((a, i) => from(project(a, i)).pipe(map((b, ii) => resultSelector(a, b, i, ii)))));
    }
    return (source) => source.lift(new ExhauseMapOperator(project));
}
class ExhauseMapOperator {
    constructor(project) {
        this.project = project;
    }
    call(subscriber, source) {
        return source.subscribe(new ExhaustMapSubscriber(subscriber, this.project));
    }
}
class ExhaustMapSubscriber extends OuterSubscriber {
    constructor(destination, project) {
        super(destination);
        this.project = project;
        this.hasSubscription = false;
        this.hasCompleted = false;
        this.index = 0;
    }
    _next(value) {
        if (!this.hasSubscription) {
            this.tryNext(value);
        }
    }
    tryNext(value) {
        let result;
        const index = this.index++;
        try {
            result = this.project(value, index);
        }
        catch (err) {
            this.destination.error(err);
            return;
        }
        this.hasSubscription = true;
        this._innerSub(result, value, index);
    }
    _innerSub(result, value, index) {
        const innerSubscriber = new InnerSubscriber(this, undefined, undefined);
        this.add(innerSubscriber);
        subscribeToResult(this, result, value, index, innerSubscriber);
    }
    _complete() {
        this.hasCompleted = true;
        if (!this.hasSubscription) {
            this.destination.complete();
        }
    }
    notifyNext(outerValue, innerValue, outerIndex, innerIndex, innerSub) {
        this.destination.next(innerValue);
    }
    notifyError(err) {
        this.destination.error(err);
    }
    notifyComplete(innerSub) {
        this.remove(innerSub);
        this.hasSubscription = false;
        if (this.hasCompleted) {
            this.destination.complete();
        }
    }
}
//# sourceMappingURL=exhaustMap.js.map