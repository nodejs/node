import { MonoTypeOperatorFunction } from '../types';
/**
 * Returns an Observable that repeats the stream of items emitted by the source Observable at most count times.
 *
 * ![](repeat.png)
 *
 * @param {number} [count] The number of times the source Observable items are repeated, a count of 0 will yield
 * an empty Observable.
 * @return {Observable} An Observable that repeats the stream of items emitted by the source Observable at most
 * count times.
 * @method repeat
 * @owner Observable
 */
export declare function repeat<T>(count?: number): MonoTypeOperatorFunction<T>;
