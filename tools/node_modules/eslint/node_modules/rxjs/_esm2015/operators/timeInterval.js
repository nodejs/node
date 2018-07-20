import { Subscriber } from '../Subscriber';
import { async } from '../scheduler/async';
export function timeInterval(scheduler = async) {
    return (source) => source.lift(new TimeIntervalOperator(scheduler));
}
export class TimeInterval {
    constructor(value, interval) {
        this.value = value;
        this.interval = interval;
    }
}
;
class TimeIntervalOperator {
    constructor(scheduler) {
        this.scheduler = scheduler;
    }
    call(observer, source) {
        return source.subscribe(new TimeIntervalSubscriber(observer, this.scheduler));
    }
}
/**
 * We need this JSDoc comment for affecting ESDoc.
 * @ignore
 * @extends {Ignored}
 */
class TimeIntervalSubscriber extends Subscriber {
    constructor(destination, scheduler) {
        super(destination);
        this.scheduler = scheduler;
        this.lastTime = 0;
        this.lastTime = scheduler.now();
    }
    _next(value) {
        let now = this.scheduler.now();
        let span = now - this.lastTime;
        this.lastTime = now;
        this.destination.next(new TimeInterval(value, span));
    }
}
//# sourceMappingURL=timeInterval.js.map