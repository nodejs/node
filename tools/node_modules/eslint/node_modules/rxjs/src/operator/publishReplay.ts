import { Observable } from '../Observable';
import { IScheduler } from '../Scheduler';
import { ConnectableObservable } from '../observable/ConnectableObservable';
import { publishReplay as higherOrder } from '../operators/publishReplay';
import { OperatorFunction, MonoTypeOperatorFunction } from '../interfaces';

/* tslint:disable:max-line-length */
export function publishReplay<T>(this: Observable<T>, bufferSize?: number, windowTime?: number, scheduler?: IScheduler): ConnectableObservable<T>;
export function publishReplay<T>(this: Observable<T>, bufferSize?: number, windowTime?: number, selector?: MonoTypeOperatorFunction<T>, scheduler?: IScheduler): Observable<T>;
export function publishReplay<T, R>(this: Observable<T>, bufferSize?: number, windowTime?: number, selector?: OperatorFunction<T, R>): Observable<R>;
/* tslint:enable:max-line-length */

/**
 * @param bufferSize
 * @param windowTime
 * @param selectorOrScheduler
 * @param scheduler
 * @return {Observable<T> | ConnectableObservable<T>}
 * @method publishReplay
 * @owner Observable
 */
export function publishReplay<T, R>(this: Observable<T>, bufferSize?: number,
                                    windowTime?: number,
                                    selectorOrScheduler?: IScheduler | OperatorFunction<T, R>,
                                    scheduler?: IScheduler): Observable<R> | ConnectableObservable<R> {

  return higherOrder<T, R>(bufferSize, windowTime, selectorOrScheduler as any, scheduler)(this);
}
