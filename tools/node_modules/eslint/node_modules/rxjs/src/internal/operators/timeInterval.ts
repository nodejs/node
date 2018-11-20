
import { Observable } from '../Observable';
import { async } from '../scheduler/async';
import { SchedulerLike, OperatorFunction } from '../types';
import { scan } from './scan';
import { defer } from '../observable/defer';
import { map } from './map';

export function timeInterval<T>(scheduler: SchedulerLike = async): OperatorFunction<T, TimeInterval<T>> {
  return (source: Observable<T>) => defer(() => {
    return source.pipe(
      // HACK: the typings seem off with scan
      scan(
        ({ current }, value) => ({ value, current: scheduler.now(), last: current }),
        { current: scheduler.now(), value: undefined,  last: undefined }
      ) as any,
      map<any, TimeInterval<T>>(({ current, last, value }) => new TimeInterval(value, current - last)),
    );
  });
}

export class TimeInterval<T> {
  constructor(public value: T, public interval: number) {}
}
