import { reduce } from './reduce';
import { MonoTypeOperatorFunction } from '../types';

/**
 * The Max operator operates on an Observable that emits numbers (or items that can be compared with a provided function),
 * and when source Observable completes it emits a single item: the item with the largest value.
 *
 * ![](max.png)
 *
 * ## Examples
 * Get the maximal value of a series of numbers
 * ```javascript
 * of(5, 4, 7, 2, 8).pipe(
 *   max(),
 * )
 * .subscribe(x => console.log(x)); // -> 8
 * ```
 *
 * Use a comparer function to get the maximal item
 * ```typescript
 * interface Person {
 *   age: number,
 *   name: string
 * }
 * of<Person>(
 *   {age: 7, name: 'Foo'},
 *   {age: 5, name: 'Bar'},
 *   {age: 9, name: 'Beer'},
 * ).pipe(
 *   max<Person>((a: Person, b: Person) => a.age < b.age ? -1 : 1),
 * )
 * .subscribe((x: Person) => console.log(x.name)); // -> 'Beer'
 * ```
 *
 * @see {@link min}
 *
 * @param {Function} [comparer] - Optional comparer function that it will use instead of its default to compare the
 * value of two items.
 * @return {Observable} An Observable that emits item with the largest value.
 * @method max
 * @owner Observable
 */
export function max<T>(comparer?: (x: T, y: T) => number): MonoTypeOperatorFunction<T> {
  const max: (x: T, y: T) => T = (typeof comparer === 'function')
    ? (x, y) => comparer(x, y) > 0 ? x : y
    : (x, y) => x > y ? x : y;

  return reduce(max);
}
