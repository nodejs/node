import { Observable } from '../Observable';
import { ReplaySubject } from '../ReplaySubject';
import { IScheduler } from '../Scheduler';
import { multicast } from './multicast';
import { ConnectableObservable } from '../observable/ConnectableObservable';
import { UnaryFunction, MonoTypeOperatorFunction, OperatorFunction } from '../interfaces';

/* tslint:disable:max-line-length */
export function publishReplay<T>(bufferSize?: number, windowTime?: number, scheduler?: IScheduler): UnaryFunction<Observable<T>, ConnectableObservable<T>>;
export function publishReplay<T>(bufferSize?: number, windowTime?: number, selector?: MonoTypeOperatorFunction<T>, scheduler?: IScheduler): MonoTypeOperatorFunction<T>;
export function publishReplay<T, R>(bufferSize?: number, windowTime?: number, selector?: OperatorFunction<T, R>, scheduler?: IScheduler): OperatorFunction<T, R>;
/* tslint:enable:max-line-length */

export function publishReplay<T, R>(bufferSize?: number,
                                    windowTime?: number,
                                    selectorOrScheduler?: IScheduler | OperatorFunction<T, R>,
                                    scheduler?: IScheduler): UnaryFunction<Observable<T>, ConnectableObservable<R> | Observable<R>> {

  if (selectorOrScheduler && typeof selectorOrScheduler !== 'function') {
    scheduler = selectorOrScheduler;
  }

  const selector = typeof selectorOrScheduler === 'function' ? selectorOrScheduler : undefined;
  const subject = new ReplaySubject<T>(bufferSize, windowTime, scheduler);

  return (source: Observable<T>) => multicast(() => subject, selector)(source) as Observable<R> | ConnectableObservable<R>;
}
