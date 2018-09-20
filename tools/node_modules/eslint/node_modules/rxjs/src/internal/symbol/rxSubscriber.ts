export const rxSubscriber =
  (typeof Symbol === 'function' && typeof Symbol.for === 'function')
    ? Symbol.for('rxSubscriber')
    : '@@rxSubscriber';

/**
 * @deprecated use rxSubscriber instead
 */
export const $$rxSubscriber = rxSubscriber;
