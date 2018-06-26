import { IScheduler } from '../Scheduler';
import { Action } from '../scheduler/Action';
import { Operator } from '../Operator';
import { async } from '../scheduler/async';
import { Observable } from '../Observable';
import { Subscriber } from '../Subscriber';
import { Subscription } from '../Subscription';
import { isScheduler } from '../util/isScheduler';
import { OperatorFunction } from '../interfaces';

/* tslint:disable:max-line-length */
export function bufferTime<T>(bufferTimeSpan: number, scheduler?: IScheduler): OperatorFunction<T, T[]>;
export function bufferTime<T>(bufferTimeSpan: number, bufferCreationInterval: number, scheduler?: IScheduler): OperatorFunction<T, T[]>;
export function bufferTime<T>(bufferTimeSpan: number, bufferCreationInterval: number, maxBufferSize: number, scheduler?: IScheduler): OperatorFunction<T, T[]>;
/* tslint:enable:max-line-length */

/**
 * Buffers the source Observable values for a specific time period.
 *
 * <span class="informal">Collects values from the past as an array, and emits
 * those arrays periodically in time.</span>
 *
 * <img src="./img/bufferTime.png" width="100%">
 *
 * Buffers values from the source for a specific time duration `bufferTimeSpan`.
 * Unless the optional argument `bufferCreationInterval` is given, it emits and
 * resets the buffer every `bufferTimeSpan` milliseconds. If
 * `bufferCreationInterval` is given, this operator opens the buffer every
 * `bufferCreationInterval` milliseconds and closes (emits and resets) the
 * buffer every `bufferTimeSpan` milliseconds. When the optional argument
 * `maxBufferSize` is specified, the buffer will be closed either after
 * `bufferTimeSpan` milliseconds or when it contains `maxBufferSize` elements.
 *
 * @example <caption>Every second, emit an array of the recent click events</caption>
 * var clicks = Rx.Observable.fromEvent(document, 'click');
 * var buffered = clicks.bufferTime(1000);
 * buffered.subscribe(x => console.log(x));
 *
 * @example <caption>Every 5 seconds, emit the click events from the next 2 seconds</caption>
 * var clicks = Rx.Observable.fromEvent(document, 'click');
 * var buffered = clicks.bufferTime(2000, 5000);
 * buffered.subscribe(x => console.log(x));
 *
 * @see {@link buffer}
 * @see {@link bufferCount}
 * @see {@link bufferToggle}
 * @see {@link bufferWhen}
 * @see {@link windowTime}
 *
 * @param {number} bufferTimeSpan The amount of time to fill each buffer array.
 * @param {number} [bufferCreationInterval] The interval at which to start new
 * buffers.
 * @param {number} [maxBufferSize] The maximum buffer size.
 * @param {Scheduler} [scheduler=async] The scheduler on which to schedule the
 * intervals that determine buffer boundaries.
 * @return {Observable<T[]>} An observable of arrays of buffered values.
 * @method bufferTime
 * @owner Observable
 */
export function bufferTime<T>(bufferTimeSpan: number): OperatorFunction<T, T[]> {
  let length: number = arguments.length;

  let scheduler: IScheduler = async;
  if (isScheduler(arguments[arguments.length - 1])) {
    scheduler = arguments[arguments.length - 1];
    length--;
  }

  let bufferCreationInterval: number = null;
  if (length >= 2) {
    bufferCreationInterval = arguments[1];
  }

  let maxBufferSize: number = Number.POSITIVE_INFINITY;
  if (length >= 3) {
    maxBufferSize = arguments[2];
  }

  return function bufferTimeOperatorFunction(source: Observable<T>) {
    return source.lift(new BufferTimeOperator<T>(bufferTimeSpan, bufferCreationInterval, maxBufferSize, scheduler));
  };
}

class BufferTimeOperator<T> implements Operator<T, T[]> {
  constructor(private bufferTimeSpan: number,
              private bufferCreationInterval: number,
              private maxBufferSize: number,
              private scheduler: IScheduler) {
  }

  call(subscriber: Subscriber<T[]>, source: any): any {
    return source.subscribe(new BufferTimeSubscriber(
      subscriber, this.bufferTimeSpan, this.bufferCreationInterval, this.maxBufferSize, this.scheduler
    ));
  }
}

class Context<T> {
  buffer: T[] = [];
  closeAction: Subscription;
}

type CreationState<T> = {
  bufferTimeSpan: number;
  bufferCreationInterval: number,
  subscriber: BufferTimeSubscriber<T>;
  scheduler: IScheduler;
};

/**
 * We need this JSDoc comment for affecting ESDoc.
 * @ignore
 * @extends {Ignored}
 */
class BufferTimeSubscriber<T> extends Subscriber<T> {
  private contexts: Array<Context<T>> = [];
  private timespanOnly: boolean;

  constructor(destination: Subscriber<T[]>,
              private bufferTimeSpan: number,
              private bufferCreationInterval: number,
              private maxBufferSize: number,
              private scheduler: IScheduler) {
    super(destination);
    const context = this.openContext();
    this.timespanOnly = bufferCreationInterval == null || bufferCreationInterval < 0;
    if (this.timespanOnly) {
      const timeSpanOnlyState = { subscriber: this, context, bufferTimeSpan };
      this.add(context.closeAction = scheduler.schedule(dispatchBufferTimeSpanOnly, bufferTimeSpan, timeSpanOnlyState));
    } else {
      const closeState = { subscriber: this, context };
      const creationState: CreationState<T> = { bufferTimeSpan, bufferCreationInterval, subscriber: this, scheduler };
      this.add(context.closeAction = scheduler.schedule(dispatchBufferClose, bufferTimeSpan, closeState));
      this.add(scheduler.schedule(dispatchBufferCreation, bufferCreationInterval, creationState));
    }
  }

  protected _next(value: T) {
    const contexts = this.contexts;
    const len = contexts.length;
    let filledBufferContext: Context<T>;
    for (let i = 0; i < len; i++) {
      const context = contexts[i];
      const buffer = context.buffer;
      buffer.push(value);
      if (buffer.length == this.maxBufferSize) {
        filledBufferContext = context;
      }
    }

    if (filledBufferContext) {
      this.onBufferFull(filledBufferContext);
    }
  }

  protected _error(err: any) {
    this.contexts.length = 0;
    super._error(err);
  }

  protected _complete() {
    const { contexts, destination } = this;
    while (contexts.length > 0) {
      const context = contexts.shift();
      destination.next(context.buffer);
    }
    super._complete();
  }

  /** @deprecated internal use only */ _unsubscribe() {
    this.contexts = null;
  }

  protected onBufferFull(context: Context<T>) {
    this.closeContext(context);
    const closeAction = context.closeAction;
    closeAction.unsubscribe();
    this.remove(closeAction);

    if (!this.closed && this.timespanOnly) {
      context = this.openContext();
      const bufferTimeSpan = this.bufferTimeSpan;
      const timeSpanOnlyState = { subscriber: this, context, bufferTimeSpan };
      this.add(context.closeAction = this.scheduler.schedule(dispatchBufferTimeSpanOnly, bufferTimeSpan, timeSpanOnlyState));
    }
  }

  openContext(): Context<T> {
    const context: Context<T> = new Context<T>();
    this.contexts.push(context);
    return context;
  }

  closeContext(context: Context<T>) {
    this.destination.next(context.buffer);
    const contexts = this.contexts;

    const spliceIndex = contexts ? contexts.indexOf(context) : -1;
    if (spliceIndex >= 0) {
      contexts.splice(contexts.indexOf(context), 1);
    }
  }
}

function dispatchBufferTimeSpanOnly(this: Action<any>, state: any) {
  const subscriber: BufferTimeSubscriber<any> = state.subscriber;

  const prevContext = state.context;
  if (prevContext) {
    subscriber.closeContext(prevContext);
  }

  if (!subscriber.closed) {
    state.context = subscriber.openContext();
    state.context.closeAction = this.schedule(state, state.bufferTimeSpan);
  }
}

interface DispatchArg<T> {
  subscriber: BufferTimeSubscriber<T>;
  context: Context<T>;
}

function dispatchBufferCreation<T>(this: Action<CreationState<T>>, state: CreationState<T>) {
  const { bufferCreationInterval, bufferTimeSpan, subscriber, scheduler } = state;
  const context = subscriber.openContext();
  const action = <Action<CreationState<T>>>this;
  if (!subscriber.closed) {
    subscriber.add(context.closeAction = scheduler.schedule<DispatchArg<T>>(dispatchBufferClose, bufferTimeSpan, { subscriber, context }));
    action.schedule(state, bufferCreationInterval);
  }
}

function dispatchBufferClose<T>(arg: DispatchArg<T>) {
  const { subscriber, context } = arg;
  subscriber.closeContext(context);
}
