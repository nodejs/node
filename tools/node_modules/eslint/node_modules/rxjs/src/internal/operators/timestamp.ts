
import { async } from '../scheduler/async';
import { OperatorFunction, SchedulerLike, Timestamp as TimestampInterface } from '../types';
import { map } from './map';

/**
 * @param scheduler
 * @return {Observable<Timestamp<any>>|WebSocketSubject<T>|Observable<T>}
 * @method timestamp
 * @owner Observable
 */
export function timestamp<T>(scheduler: SchedulerLike = async): OperatorFunction<T, Timestamp<T>> {
  return map((value: T) => new Timestamp(value, scheduler.now()));
  // return (source: Observable<T>) => source.lift(new TimestampOperator(scheduler));
}

export class Timestamp<T> implements TimestampInterface<T> {
  constructor(public value: T, public timestamp: number) {
  }
}
