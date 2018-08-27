import { AnonymousSubject } from '../../Subject';
import { Subscriber } from '../../Subscriber';
import { Observable } from '../../Observable';
import { Subscription } from '../../Subscription';
import { Operator } from '../../Operator';
import { Observer, NextObserver } from '../../Observer';
export interface WebSocketSubjectConfig {
    url: string;
    protocol?: string | Array<string>;
    resultSelector?: <T>(e: MessageEvent) => T;
    openObserver?: NextObserver<Event>;
    closeObserver?: NextObserver<CloseEvent>;
    closingObserver?: NextObserver<void>;
    WebSocketCtor?: {
        new (url: string, protocol?: string | Array<string>): WebSocket;
    };
    binaryType?: 'blob' | 'arraybuffer';
}
/**
 * We need this JSDoc comment for affecting ESDoc.
 * @extends {Ignored}
 * @hide true
 */
export declare class WebSocketSubject<T> extends AnonymousSubject<T> {
    url: string;
    protocol: string | Array<string>;
    socket: WebSocket;
    openObserver: NextObserver<Event>;
    closeObserver: NextObserver<CloseEvent>;
    closingObserver: NextObserver<void>;
    WebSocketCtor: {
        new (url: string, protocol?: string | Array<string>): WebSocket;
    };
    binaryType?: 'blob' | 'arraybuffer';
    private _output;
    resultSelector(e: MessageEvent): any;
    /**
     * Wrapper around the w3c-compatible WebSocket object provided by the browser.
     *
     * @example <caption>Wraps browser WebSocket</caption>
     *
     * let socket$ = Observable.webSocket('ws://localhost:8081');
     *
     * socket$.subscribe(
     *    (msg) => console.log('message received: ' + msg),
     *    (err) => console.log(err),
     *    () => console.log('complete')
     *  );
     *
     * socket$.next(JSON.stringify({ op: 'hello' }));
     *
     * @example <caption>Wraps WebSocket from nodejs-websocket (using node.js)</caption>
     *
     * import { w3cwebsocket } from 'websocket';
     *
     * let socket$ = Observable.webSocket({
     *   url: 'ws://localhost:8081',
     *   WebSocketCtor: w3cwebsocket
     * });
     *
     * socket$.subscribe(
     *    (msg) => console.log('message received: ' + msg),
     *    (err) => console.log(err),
     *    () => console.log('complete')
     *  );
     *
     * socket$.next(JSON.stringify({ op: 'hello' }));
     *
     * @param {string | WebSocketSubjectConfig} urlConfigOrSource the source of the websocket as an url or a structure defining the websocket object
     * @return {WebSocketSubject}
     * @static true
     * @name webSocket
     * @owner Observable
     */
    static create<T>(urlConfigOrSource: string | WebSocketSubjectConfig): WebSocketSubject<T>;
    constructor(urlConfigOrSource: string | WebSocketSubjectConfig | Observable<T>, destination?: Observer<T>);
    lift<R>(operator: Operator<T, R>): WebSocketSubject<R>;
    private _resetState();
    multiplex(subMsg: () => any, unsubMsg: () => any, messageFilter: (value: T) => boolean): Observable<any>;
    private _connectSocket();
    /** @deprecated internal use only */ _subscribe(subscriber: Subscriber<T>): Subscription;
    unsubscribe(): void;
}
