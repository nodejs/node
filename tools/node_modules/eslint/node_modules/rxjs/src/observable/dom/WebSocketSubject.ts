import { Subject, AnonymousSubject } from '../../Subject';
import { Subscriber } from '../../Subscriber';
import { Observable } from '../../Observable';
import { Subscription } from '../../Subscription';
import { Operator } from '../../Operator';
import { root } from '../../util/root';
import { ReplaySubject } from '../../ReplaySubject';
import { Observer, NextObserver } from '../../Observer';
import { tryCatch } from '../../util/tryCatch';
import { errorObject } from '../../util/errorObject';
import { assign } from '../../util/assign';

export interface WebSocketSubjectConfig {
  url: string;
  protocol?: string | Array<string>;
  resultSelector?: <T>(e: MessageEvent) => T;
  openObserver?: NextObserver<Event>;
  closeObserver?: NextObserver<CloseEvent>;
  closingObserver?: NextObserver<void>;
  WebSocketCtor?: { new(url: string, protocol?: string|Array<string>): WebSocket };
  binaryType?: 'blob' | 'arraybuffer';
}

/**
 * We need this JSDoc comment for affecting ESDoc.
 * @extends {Ignored}
 * @hide true
 */
export class WebSocketSubject<T> extends AnonymousSubject<T> {

  url: string;
  protocol: string|Array<string>;
  socket: WebSocket;
  openObserver: NextObserver<Event>;
  closeObserver: NextObserver<CloseEvent>;
  closingObserver: NextObserver<void>;
  WebSocketCtor: { new(url: string, protocol?: string|Array<string>): WebSocket };
  binaryType?: 'blob' | 'arraybuffer';

  private _output: Subject<T>;

  resultSelector(e: MessageEvent) {
    return JSON.parse(e.data);
  }

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
  static create<T>(urlConfigOrSource: string | WebSocketSubjectConfig): WebSocketSubject<T> {
    return new WebSocketSubject<T>(urlConfigOrSource);
  }

  constructor(urlConfigOrSource: string | WebSocketSubjectConfig | Observable<T>, destination?: Observer<T>) {
    if (urlConfigOrSource instanceof Observable) {
      super(destination, <Observable<T>> urlConfigOrSource);
    } else {
      super();
      this.WebSocketCtor = root.WebSocket;
      this._output = new Subject<T>();
      if (typeof urlConfigOrSource === 'string') {
        this.url = urlConfigOrSource;
      } else {
        // WARNING: config object could override important members here.
        assign(this, urlConfigOrSource);
      }
      if (!this.WebSocketCtor) {
        throw new Error('no WebSocket constructor can be found');
      }
      this.destination = new ReplaySubject();
    }
  }

  lift<R>(operator: Operator<T, R>): WebSocketSubject<R> {
    const sock = new WebSocketSubject<R>(this, <any> this.destination);
    sock.operator = operator;
    return sock;
  }

  private _resetState() {
    this.socket = null;
    if (!this.source) {
      this.destination = new ReplaySubject();
    }
    this._output = new Subject<T>();
  }

  // TODO: factor this out to be a proper Operator/Subscriber implementation and eliminate closures
  multiplex(subMsg: () => any, unsubMsg: () => any, messageFilter: (value: T) => boolean) {
    const self = this;
    return new Observable((observer: Observer<any>) => {
      const result = tryCatch(subMsg)();
      if (result === errorObject) {
        observer.error(errorObject.e);
      } else {
        self.next(result);
      }

      let subscription = self.subscribe(x => {
        const result = tryCatch(messageFilter)(x);
        if (result === errorObject) {
          observer.error(errorObject.e);
        } else if (result) {
          observer.next(x);
        }
      },
        err => observer.error(err),
        () => observer.complete());

      return () => {
        const result = tryCatch(unsubMsg)();
        if (result === errorObject) {
          observer.error(errorObject.e);
        } else {
          self.next(result);
        }
        subscription.unsubscribe();
      };
    });
  }

  private _connectSocket() {
    const { WebSocketCtor } = this;
    const observer = this._output;

    let socket: WebSocket = null;
    try {
      socket = this.protocol ?
        new WebSocketCtor(this.url, this.protocol) :
        new WebSocketCtor(this.url);
      this.socket = socket;
      if (this.binaryType) {
        this.socket.binaryType = this.binaryType;
      }
    } catch (e) {
      observer.error(e);
      return;
    }

    const subscription = new Subscription(() => {
      this.socket = null;
      if (socket && socket.readyState === 1) {
        socket.close();
      }
    });

    socket.onopen = (e: Event) => {
      const openObserver = this.openObserver;
      if (openObserver) {
        openObserver.next(e);
      }

      const queue = this.destination;

      this.destination = Subscriber.create(
        (x) => socket.readyState === 1 && socket.send(x),
        (e) => {
          const closingObserver = this.closingObserver;
          if (closingObserver) {
            closingObserver.next(undefined);
          }
          if (e && e.code) {
            socket.close(e.code, e.reason);
          } else {
            observer.error(new TypeError('WebSocketSubject.error must be called with an object with an error code, ' +
              'and an optional reason: { code: number, reason: string }'));
          }
          this._resetState();
        },
        ( ) => {
          const closingObserver = this.closingObserver;
          if (closingObserver) {
            closingObserver.next(undefined);
          }
          socket.close();
          this._resetState();
        }
      );

      if (queue && queue instanceof ReplaySubject) {
        subscription.add((<ReplaySubject<T>>queue).subscribe(this.destination));
      }
    };

    socket.onerror = (e: Event) => {
      this._resetState();
      observer.error(e);
    };

    socket.onclose = (e: CloseEvent) => {
      this._resetState();
      const closeObserver = this.closeObserver;
      if (closeObserver) {
        closeObserver.next(e);
      }
      if (e.wasClean) {
        observer.complete();
      } else {
        observer.error(e);
      }
    };

    socket.onmessage = (e: MessageEvent) => {
      const result = tryCatch(this.resultSelector)(e);
      if (result === errorObject) {
        observer.error(errorObject.e);
      } else {
        observer.next(result);
      }
    };
  }

  /** @deprecated internal use only */ _subscribe(subscriber: Subscriber<T>): Subscription {
    const { source } = this;
    if (source) {
      return source.subscribe(subscriber);
    }
    if (!this.socket) {
      this._connectSocket();
    }
    let subscription = new Subscription();
    subscription.add(this._output.subscribe(subscriber));
    subscription.add(() => {
      const { socket } = this;
      if (this._output.observers.length === 0) {
        if (socket && socket.readyState === 1) {
          socket.close();
        }
        this._resetState();
      }
    });
    return subscription;
  }

  unsubscribe() {
    const { source, socket } = this;
    if (socket && socket.readyState === 1) {
      socket.close();
      this._resetState();
    }
    super.unsubscribe();
    if (!source) {
      this.destination = new ReplaySubject();
    }
  }
}
