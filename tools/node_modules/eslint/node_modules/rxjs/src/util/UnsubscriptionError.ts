/**
 * An error thrown when one or more errors have occurred during the
 * `unsubscribe` of a {@link Subscription}.
 */
export class UnsubscriptionError extends Error {
  constructor(public errors: any[]) {
    super();
    const err: any = Error.call(this, errors ?
      `${errors.length} errors occurred during unsubscription:
  ${errors.map((err, i) => `${i + 1}) ${err.toString()}`).join('\n  ')}` : '');
    (<any> this).name = err.name = 'UnsubscriptionError';
    (<any> this).stack = err.stack;
    (<any> this).message = err.message;
  }
}
