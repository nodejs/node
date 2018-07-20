import { Observable } from '../Observable';
import { Subscriber } from '../Subscriber';
export declare type NodeStyleEventEmitter = {
    addListener: (eventName: string, handler: NodeEventHandler) => void;
    removeListener: (eventName: string, handler: NodeEventHandler) => void;
};
export declare type NodeEventHandler = (...args: any[]) => void;
export declare type JQueryStyleEventEmitter = {
    on: (eventName: string, handler: Function) => void;
    off: (eventName: string, handler: Function) => void;
};
export declare type EventTargetLike = EventTarget | NodeStyleEventEmitter | JQueryStyleEventEmitter | NodeList | HTMLCollection;
export declare type EventListenerOptions = {
    capture?: boolean;
    passive?: boolean;
    once?: boolean;
} | boolean;
export declare type SelectorMethodSignature<T> = (...args: Array<any>) => T;
/**
 * We need this JSDoc comment for affecting ESDoc.
 * @extends {Ignored}
 * @hide true
 */
export declare class FromEventObservable<T> extends Observable<T> {
    private sourceObj;
    private eventName;
    private selector;
    private options;
    static create<T>(target: EventTargetLike, eventName: string): Observable<T>;
    static create<T>(target: EventTargetLike, eventName: string, selector: SelectorMethodSignature<T>): Observable<T>;
    static create<T>(target: EventTargetLike, eventName: string, options: EventListenerOptions): Observable<T>;
    static create<T>(target: EventTargetLike, eventName: string, options: EventListenerOptions, selector: SelectorMethodSignature<T>): Observable<T>;
    constructor(sourceObj: EventTargetLike, eventName: string, selector?: SelectorMethodSignature<T>, options?: EventListenerOptions);
    private static setupSubscription<T>(sourceObj, eventName, handler, subscriber, options?);
    /** @deprecated internal use only */ _subscribe(subscriber: Subscriber<T>): void;
}
