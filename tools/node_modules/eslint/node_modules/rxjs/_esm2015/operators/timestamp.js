import { async } from '../scheduler/async';
import { map } from './map';
/**
 * @param scheduler
 * @return {Observable<Timestamp<any>>|WebSocketSubject<T>|Observable<T>}
 * @method timestamp
 * @owner Observable
 */
export function timestamp(scheduler = async) {
    return map((value) => new Timestamp(value, scheduler.now()));
    // return (source: Observable<T>) => source.lift(new TimestampOperator(scheduler));
}
export class Timestamp {
    constructor(value, timestamp) {
        this.value = value;
        this.timestamp = timestamp;
    }
}
;
//# sourceMappingURL=timestamp.js.map