/**
 * An error thrown when an action is invalid because the object has been
 * unsubscribed.
 *
 * @see {@link Subject}
 * @see {@link BehaviorSubject}
 *
 * @class ObjectUnsubscribedError
 */
export class ObjectUnsubscribedError extends Error {
  constructor() {
    const err: any = super('object unsubscribed');
    (<any> this).name = err.name = 'ObjectUnsubscribedError';
    (<any> this).stack = err.stack;
    (<any> this).message = err.message;
  }
}
