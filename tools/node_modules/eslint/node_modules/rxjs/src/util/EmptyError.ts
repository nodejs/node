/**
 * An error thrown when an Observable or a sequence was queried but has no
 * elements.
 *
 * @see {@link first}
 * @see {@link last}
 * @see {@link single}
 *
 * @class EmptyError
 */
export class EmptyError extends Error {
  constructor() {
    const err: any = super('no elements in sequence');
    (<any> this).name = err.name = 'EmptyError';
    (<any> this).stack = err.stack;
    (<any> this).message = err.message;
  }
}
