import { Observable } from '../Observable';
import { Notification } from '../Notification';
import { ColdObservable } from './ColdObservable';
import { HotObservable } from './HotObservable';
import { TestMessage } from './TestMessage';
import { SubscriptionLog } from './SubscriptionLog';
import { Subscription } from '../Subscription';
import { VirtualTimeScheduler, VirtualAction } from '../scheduler/VirtualTimeScheduler';

const defaultMaxFrame: number = 750;

interface FlushableTest {
  ready: boolean;
  actual?: any[];
  expected?: any[];
}

export type observableToBeFn = (marbles: string, values?: any, errorValue?: any) => void;
export type subscriptionLogsToBeFn = (marbles: string | string[]) => void;

export class TestScheduler extends VirtualTimeScheduler {
  private hotObservables: HotObservable<any>[] = [];
  private coldObservables: ColdObservable<any>[] = [];
  private flushTests: FlushableTest[] = [];

  constructor(public assertDeepEqual: (actual: any, expected: any) => boolean | void) {
    super(VirtualAction, defaultMaxFrame);
  }

  createTime(marbles: string): number {
    const indexOf: number = marbles.indexOf('|');
    if (indexOf === -1) {
      throw new Error('marble diagram for time should have a completion marker "|"');
    }
    return indexOf * TestScheduler.frameTimeFactor;
  }

  createColdObservable<T>(marbles: string, values?: any, error?: any): ColdObservable<T> {
    if (marbles.indexOf('^') !== -1) {
      throw new Error('cold observable cannot have subscription offset "^"');
    }
    if (marbles.indexOf('!') !== -1) {
      throw new Error('cold observable cannot have unsubscription marker "!"');
    }
    const messages = TestScheduler.parseMarbles(marbles, values, error);
    const cold = new ColdObservable<T>(messages, this);
    this.coldObservables.push(cold);
    return cold;
  }

  createHotObservable<T>(marbles: string, values?: any, error?: any): HotObservable<T> {
    if (marbles.indexOf('!') !== -1) {
      throw new Error('hot observable cannot have unsubscription marker "!"');
    }
    const messages = TestScheduler.parseMarbles(marbles, values, error);
    const subject = new HotObservable<T>(messages, this);
    this.hotObservables.push(subject);
    return subject;
  }

  private materializeInnerObservable(observable: Observable<any>,
                                     outerFrame: number): TestMessage[] {
    const messages: TestMessage[] = [];
    observable.subscribe((value) => {
      messages.push({ frame: this.frame - outerFrame, notification: Notification.createNext(value) });
    }, (err) => {
      messages.push({ frame: this.frame - outerFrame, notification: Notification.createError(err) });
    }, () => {
      messages.push({ frame: this.frame - outerFrame, notification: Notification.createComplete() });
    });
    return messages;
  }

  expectObservable(observable: Observable<any>,
                   unsubscriptionMarbles: string = null): ({ toBe: observableToBeFn }) {
    const actual: TestMessage[] = [];
    const flushTest: FlushableTest = { actual, ready: false };
    const unsubscriptionFrame = TestScheduler
      .parseMarblesAsSubscriptions(unsubscriptionMarbles).unsubscribedFrame;
    let subscription: Subscription;

    this.schedule(() => {
      subscription = observable.subscribe(x => {
        let value = x;
        // Support Observable-of-Observables
        if (x instanceof Observable) {
          value = this.materializeInnerObservable(value, this.frame);
        }
        actual.push({ frame: this.frame, notification: Notification.createNext(value) });
      }, (err) => {
        actual.push({ frame: this.frame, notification: Notification.createError(err) });
      }, () => {
        actual.push({ frame: this.frame, notification: Notification.createComplete() });
      });
    }, 0);

    if (unsubscriptionFrame !== Number.POSITIVE_INFINITY) {
      this.schedule(() => subscription.unsubscribe(), unsubscriptionFrame);
    }

    this.flushTests.push(flushTest);

    return {
      toBe(marbles: string, values?: any, errorValue?: any) {
        flushTest.ready = true;
        flushTest.expected = TestScheduler.parseMarbles(marbles, values, errorValue, true);
      }
    };
  }

  expectSubscriptions(actualSubscriptionLogs: SubscriptionLog[]): ({ toBe: subscriptionLogsToBeFn }) {
    const flushTest: FlushableTest = { actual: actualSubscriptionLogs, ready: false };
    this.flushTests.push(flushTest);
    return {
      toBe(marbles: string | string[]) {
        const marblesArray: string[] = (typeof marbles === 'string') ? [marbles] : marbles;
        flushTest.ready = true;
        flushTest.expected = marblesArray.map(marbles =>
          TestScheduler.parseMarblesAsSubscriptions(marbles)
        );
      }
    };
  }

  flush() {
    const hotObservables = this.hotObservables;
    while (hotObservables.length > 0) {
      hotObservables.shift().setup();
    }

    super.flush();
    const readyFlushTests = this.flushTests.filter(test => test.ready);
    while (readyFlushTests.length > 0) {
      const test = readyFlushTests.shift();
      this.assertDeepEqual(test.actual, test.expected);
    }
  }

  static parseMarblesAsSubscriptions(marbles: string): SubscriptionLog {
    if (typeof marbles !== 'string') {
      return new SubscriptionLog(Number.POSITIVE_INFINITY);
    }
    const len = marbles.length;
    let groupStart = -1;
    let subscriptionFrame = Number.POSITIVE_INFINITY;
    let unsubscriptionFrame = Number.POSITIVE_INFINITY;

    for (let i = 0; i < len; i++) {
      const frame = i * this.frameTimeFactor;
      const c = marbles[i];
      switch (c) {
        case '-':
        case ' ':
          break;
        case '(':
          groupStart = frame;
          break;
        case ')':
          groupStart = -1;
          break;
        case '^':
          if (subscriptionFrame !== Number.POSITIVE_INFINITY) {
            throw new Error('found a second subscription point \'^\' in a ' +
              'subscription marble diagram. There can only be one.');
          }
          subscriptionFrame = groupStart > -1 ? groupStart : frame;
          break;
        case '!':
          if (unsubscriptionFrame !== Number.POSITIVE_INFINITY) {
            throw new Error('found a second subscription point \'^\' in a ' +
              'subscription marble diagram. There can only be one.');
          }
          unsubscriptionFrame = groupStart > -1 ? groupStart : frame;
          break;
        default:
          throw new Error('there can only be \'^\' and \'!\' markers in a ' +
            'subscription marble diagram. Found instead \'' + c + '\'.');
      }
    }

    if (unsubscriptionFrame < 0) {
      return new SubscriptionLog(subscriptionFrame);
    } else {
      return new SubscriptionLog(subscriptionFrame, unsubscriptionFrame);
    }
  }

  static parseMarbles(marbles: string,
                      values?: any,
                      errorValue?: any,
                      materializeInnerObservables: boolean = false): TestMessage[] {
    if (marbles.indexOf('!') !== -1) {
      throw new Error('conventional marble diagrams cannot have the ' +
        'unsubscription marker "!"');
    }
    const len = marbles.length;
    const testMessages: TestMessage[] = [];
    const subIndex = marbles.indexOf('^');
    const frameOffset = subIndex === -1 ? 0 : (subIndex * -this.frameTimeFactor);
    const getValue = typeof values !== 'object' ?
      (x: any) => x :
      (x: any) => {
        // Support Observable-of-Observables
        if (materializeInnerObservables && values[x] instanceof ColdObservable) {
          return values[x].messages;
        }
        return values[x];
      };
    let groupStart = -1;

    for (let i = 0; i < len; i++) {
      const frame = i * this.frameTimeFactor + frameOffset;
      let notification: Notification<any>;
      const c = marbles[i];
      switch (c) {
        case '-':
        case ' ':
          break;
        case '(':
          groupStart = frame;
          break;
        case ')':
          groupStart = -1;
          break;
        case '|':
          notification = Notification.createComplete();
          break;
        case '^':
          break;
        case '#':
          notification = Notification.createError(errorValue || 'error');
          break;
        default:
          notification = Notification.createNext(getValue(c));
          break;
      }

      if (notification) {
        testMessages.push({ frame: groupStart > -1 ? groupStart : frame, notification });
      }
    }
    return testMessages;
  }
}
