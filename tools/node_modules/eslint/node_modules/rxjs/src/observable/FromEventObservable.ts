import { Observable } from '../Observable';
import { tryCatch } from '../util/tryCatch';
import { isFunction } from '../util/isFunction';
import { errorObject } from '../util/errorObject';
import { Subscription } from '../Subscription';
import { Subscriber } from '../Subscriber';

const toString: Function = Object.prototype.toString;

export type NodeStyleEventEmitter = {
  addListener: (eventName: string, handler: NodeEventHandler) => void;
  removeListener: (eventName: string, handler: NodeEventHandler) => void;
};

export type NodeEventHandler = (...args: any[]) => void;

function isNodeStyleEventEmitter(sourceObj: any): sourceObj is NodeStyleEventEmitter {
  return !!sourceObj && typeof sourceObj.addListener === 'function' && typeof sourceObj.removeListener === 'function';
}

export type JQueryStyleEventEmitter = {
  on: (eventName: string, handler: Function) => void;
  off: (eventName: string, handler: Function) => void;
};
function isJQueryStyleEventEmitter(sourceObj: any): sourceObj is JQueryStyleEventEmitter {
  return !!sourceObj && typeof sourceObj.on === 'function' && typeof sourceObj.off === 'function';
}

function isNodeList(sourceObj: any): sourceObj is NodeList {
  return !!sourceObj && toString.call(sourceObj) === '[object NodeList]';
}

function isHTMLCollection(sourceObj: any): sourceObj is HTMLCollection {
  return !!sourceObj && toString.call(sourceObj) === '[object HTMLCollection]';
}

function isEventTarget(sourceObj: any): sourceObj is EventTarget {
  return !!sourceObj && typeof sourceObj.addEventListener === 'function' && typeof sourceObj.removeEventListener === 'function';
}

export type EventTargetLike = EventTarget | NodeStyleEventEmitter | JQueryStyleEventEmitter | NodeList | HTMLCollection;

export type EventListenerOptions = {
  capture?: boolean;
  passive?: boolean;
  once?: boolean;
} | boolean;

export type SelectorMethodSignature<T> = (...args: Array<any>) => T;

/**
 * We need this JSDoc comment for affecting ESDoc.
 * @extends {Ignored}
 * @hide true
 */
export class FromEventObservable<T> extends Observable<T> {

  /* tslint:disable:max-line-length */
  static create<T>(target: EventTargetLike, eventName: string): Observable<T>;
  static create<T>(target: EventTargetLike, eventName: string, selector: SelectorMethodSignature<T>): Observable<T>;
  static create<T>(target: EventTargetLike, eventName: string, options: EventListenerOptions): Observable<T>;
  static create<T>(target: EventTargetLike, eventName: string, options: EventListenerOptions, selector: SelectorMethodSignature<T>): Observable<T>;
  /* tslint:enable:max-line-length */

  /**
   * Creates an Observable that emits events of a specific type coming from the
   * given event target.
   *
   * <span class="informal">Creates an Observable from DOM events, or Node.js
   * EventEmitter events or others.</span>
   *
   * <img src="./img/fromEvent.png" width="100%">
   *
   * `fromEvent` accepts as a first argument event target, which is an object with methods
   * for registering event handler functions. As a second argument it takes string that indicates
   * type of event we want to listen for. `fromEvent` supports selected types of event targets,
   * which are described in detail below. If your event target does not match any of the ones listed,
   * you should use {@link fromEventPattern}, which can be used on arbitrary APIs.
   * When it comes to APIs supported by `fromEvent`, their methods for adding and removing event
   * handler functions have different names, but they all accept a string describing event type
   * and function itself, which will be called whenever said event happens.
   *
   * Every time resulting Observable is subscribed, event handler function will be registered
   * to event target on given event type. When that event fires, value
   * passed as a first argument to registered function will be emitted by output Observable.
   * When Observable is unsubscribed, function will be unregistered from event target.
   *
   * Note that if event target calls registered function with more than one argument, second
   * and following arguments will not appear in resulting stream. In order to get access to them,
   * you can pass to `fromEvent` optional project function, which will be called with all arguments
   * passed to event handler. Output Observable will then emit value returned by project function,
   * instead of the usual value.
   *
   * Remember that event targets listed below are checked via duck typing. It means that
   * no matter what kind of object you have and no matter what environment you work in,
   * you can safely use `fromEvent` on that object if it exposes described methods (provided
   * of course they behave as was described above). So for example if Node.js library exposes
   * event target which has the same method names as DOM EventTarget, `fromEvent` is still
   * a good choice.
   *
   * If the API you use is more callback then event handler oriented (subscribed
   * callback function fires only once and thus there is no need to manually
   * unregister it), you should use {@link bindCallback} or {@link bindNodeCallback}
   * instead.
   *
   * `fromEvent` supports following types of event targets:
   *
   * **DOM EventTarget**
   *
   * This is an object with `addEventListener` and `removeEventListener` methods.
   *
   * In the browser, `addEventListener` accepts - apart from event type string and event
   * handler function arguments - optional third parameter, which is either an object or boolean,
   * both used for additional configuration how and when passed function will be called. When
   * `fromEvent` is used with event target of that type, you can provide this values
   * as third parameter as well.
   *
   * **Node.js EventEmitter**
   *
   * An object with `addListener` and `removeListener` methods.
   *
   * **JQuery-style event target**
   *
   * An object with `on` and `off` methods
   *
   * **DOM NodeList**
   *
   * List of DOM Nodes, returned for example by `document.querySelectorAll` or `Node.childNodes`.
   *
   * Although this collection is not event target in itself, `fromEvent` will iterate over all Nodes
   * it contains and install event handler function in every of them. When returned Observable
   * is unsubscribed, function will be removed from all Nodes.
   *
   * **DOM HtmlCollection**
   *
   * Just as in case of NodeList it is a collection of DOM nodes. Here as well event handler function is
   * installed and removed in each of elements.
   *
   *
   * @example <caption>Emits clicks happening on the DOM document</caption>
   * var clicks = Rx.Observable.fromEvent(document, 'click');
   * clicks.subscribe(x => console.log(x));
   *
   * // Results in:
   * // MouseEvent object logged to console every time a click
   * // occurs on the document.
   *
   *
   * @example <caption>Use addEventListener with capture option</caption>
   * var clicksInDocument = Rx.Observable.fromEvent(document, 'click', true); // note optional configuration parameter
   *                                                                          // which will be passed to addEventListener
   * var clicksInDiv = Rx.Observable.fromEvent(someDivInDocument, 'click');
   *
   * clicksInDocument.subscribe(() => console.log('document'));
   * clicksInDiv.subscribe(() => console.log('div'));
   *
   * // By default events bubble UP in DOM tree, so normally
   * // when we would click on div in document
   * // "div" would be logged first and then "document".
   * // Since we specified optional `capture` option, document
   * // will catch event when it goes DOWN DOM tree, so console
   * // will log "document" and then "div".
   *
   * @see {@link bindCallback}
   * @see {@link bindNodeCallback}
   * @see {@link fromEventPattern}
   *
   * @param {EventTargetLike} target The DOM EventTarget, Node.js
   * EventEmitter, JQuery-like event target, NodeList or HTMLCollection to attach the event handler to.
   * @param {string} eventName The event name of interest, being emitted by the
   * `target`.
   * @param {EventListenerOptions} [options] Options to pass through to addEventListener
   * @param {SelectorMethodSignature<T>} [selector] An optional function to
   * post-process results. It takes the arguments from the event handler and
   * should return a single value.
   * @return {Observable<T>}
   * @static true
   * @name fromEvent
   * @owner Observable
   */
  static create<T>(target: EventTargetLike,
                   eventName: string,
                   options?: EventListenerOptions | SelectorMethodSignature<T>,
                   selector?: SelectorMethodSignature<T>): Observable<T> {
    if (isFunction(options)) {
      selector = <any>options;
      options = undefined;
    }
    return new FromEventObservable(target, eventName, selector, options as EventListenerOptions | undefined);
  }

  constructor(private sourceObj: EventTargetLike,
              private eventName: string,
              private selector?: SelectorMethodSignature<T>,
              private options?: EventListenerOptions) {
    super();
  }

  private static setupSubscription<T>(sourceObj: EventTargetLike,
                                      eventName: string,
                                      handler: Function,
                                      subscriber: Subscriber<T>,
                                      options?: EventListenerOptions) {
    let unsubscribe: () => void;
    if (isNodeList(sourceObj) || isHTMLCollection(sourceObj)) {
      for (let i = 0, len = sourceObj.length; i < len; i++) {
        FromEventObservable.setupSubscription(sourceObj[i], eventName, handler, subscriber, options);
      }
    } else if (isEventTarget(sourceObj)) {
      const source = sourceObj;
      sourceObj.addEventListener(eventName, <EventListener>handler, <boolean>options);
      unsubscribe = () => source.removeEventListener(eventName, <EventListener>handler, <boolean>options);
    } else if (isJQueryStyleEventEmitter(sourceObj)) {
      const source = sourceObj;
      sourceObj.on(eventName, handler);
      unsubscribe = () => source.off(eventName, handler);
    } else if (isNodeStyleEventEmitter(sourceObj)) {
      const source = sourceObj;
      sourceObj.addListener(eventName, handler as NodeEventHandler);
      unsubscribe = () => source.removeListener(eventName, handler as NodeEventHandler);
    } else {
      throw new TypeError('Invalid event target');
    }

    subscriber.add(new Subscription(unsubscribe));
  }

  /** @deprecated internal use only */ _subscribe(subscriber: Subscriber<T>) {
    const sourceObj = this.sourceObj;
    const eventName = this.eventName;
    const options = this.options;
    const selector = this.selector;
    let handler = selector ? (...args: any[]) => {
      let result = tryCatch(selector)(...args);
      if (result === errorObject) {
        subscriber.error(errorObject.e);
      } else {
        subscriber.next(result);
      }
    } : (e: any) => subscriber.next(e);

    FromEventObservable.setupSubscription(sourceObj, eventName, handler, subscriber, options);
  }
}
