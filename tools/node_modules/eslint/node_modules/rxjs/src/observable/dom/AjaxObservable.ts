import { root } from '../../util/root';
import { tryCatch } from '../../util/tryCatch';
import { errorObject } from '../../util/errorObject';
import { Observable } from '../../Observable';
import { Subscriber } from '../../Subscriber';
import { TeardownLogic } from '../../Subscription';
import { map } from '../../operators/map';

export interface AjaxRequest {
  url?: string;
  body?: any;
  user?: string;
  async?: boolean;
  method?: string;
  headers?: Object;
  timeout?: number;
  password?: string;
  hasContent?: boolean;
  crossDomain?: boolean;
  withCredentials?: boolean;
  createXHR?: () => XMLHttpRequest;
  progressSubscriber?: Subscriber<any>;
  responseType?: string;
}

function getCORSRequest(this: AjaxRequest): XMLHttpRequest {
  if (root.XMLHttpRequest) {
    return new root.XMLHttpRequest();
  } else if (!!root.XDomainRequest) {
    return new root.XDomainRequest();
  } else {
    throw new Error('CORS is not supported by your browser');
  }
}

function getXMLHttpRequest(): XMLHttpRequest {
  if (root.XMLHttpRequest) {
    return new root.XMLHttpRequest();
  } else {
    let progId: string;
    try {
      const progIds = ['Msxml2.XMLHTTP', 'Microsoft.XMLHTTP', 'Msxml2.XMLHTTP.4.0'];
      for (let i = 0; i < 3; i++) {
        try {
          progId = progIds[i];
          if (new root.ActiveXObject(progId)) {
            break;
          }
        } catch (e) {
          //suppress exceptions
        }
      }
      return new root.ActiveXObject(progId);
    } catch (e) {
      throw new Error('XMLHttpRequest is not supported by your browser');
    }
  }
}

export interface AjaxCreationMethod {
  (urlOrRequest: string | AjaxRequest): Observable<AjaxResponse>;
  get(url: string, headers?: Object): Observable<AjaxResponse>;
  post(url: string, body?: any, headers?: Object): Observable<AjaxResponse>;
  put(url: string, body?: any, headers?: Object): Observable<AjaxResponse>;
  patch(url: string, body?: any, headers?: Object): Observable<AjaxResponse>;
  delete(url: string, headers?: Object): Observable<AjaxResponse>;
  getJSON<T>(url: string, headers?: Object): Observable<T>;
}

export function ajaxGet(url: string, headers: Object = null) {
  return new AjaxObservable<AjaxResponse>({ method: 'GET', url, headers });
};

export function ajaxPost(url: string, body?: any, headers?: Object): Observable<AjaxResponse> {
  return new AjaxObservable<AjaxResponse>({ method: 'POST', url, body, headers });
};

export function ajaxDelete(url: string, headers?: Object): Observable<AjaxResponse> {
  return new AjaxObservable<AjaxResponse>({ method: 'DELETE', url, headers });
};

export function ajaxPut(url: string, body?: any, headers?: Object): Observable<AjaxResponse> {
  return new AjaxObservable<AjaxResponse>({ method: 'PUT', url, body, headers });
};

export function ajaxPatch(url: string, body?: any, headers?: Object): Observable<AjaxResponse> {
  return new AjaxObservable<AjaxResponse>({ method: 'PATCH', url, body, headers });
};

const mapResponse = map((x: AjaxResponse, index: number) => x.response);

export function ajaxGetJSON<T>(url: string, headers?: Object): Observable<T> {
  return mapResponse(
    new AjaxObservable<AjaxResponse>({
      method: 'GET',
      url,
      responseType: 'json',
      headers
    })
  );
};

/**
 * We need this JSDoc comment for affecting ESDoc.
 * @extends {Ignored}
 * @hide true
 */
export class AjaxObservable<T> extends Observable<T> {
  /**
   * Creates an observable for an Ajax request with either a request object with
   * url, headers, etc or a string for a URL.
   *
   * @example
   * source = Rx.Observable.ajax('/products');
   * source = Rx.Observable.ajax({ url: 'products', method: 'GET' });
   *
   * @param {string|Object} request Can be one of the following:
   *   A string of the URL to make the Ajax call.
   *   An object with the following properties
   *   - url: URL of the request
   *   - body: The body of the request
   *   - method: Method of the request, such as GET, POST, PUT, PATCH, DELETE
   *   - async: Whether the request is async
   *   - headers: Optional headers
   *   - crossDomain: true if a cross domain request, else false
   *   - createXHR: a function to override if you need to use an alternate
   *   XMLHttpRequest implementation.
   *   - resultSelector: a function to use to alter the output value type of
   *   the Observable. Gets {@link AjaxResponse} as an argument.
   * @return {Observable} An observable sequence containing the XMLHttpRequest.
   * @static true
   * @name ajax
   * @owner Observable
  */
  static create: AjaxCreationMethod = (() => {
    const create: any = (urlOrRequest: string | AjaxRequest) => {
      return new AjaxObservable(urlOrRequest);
    };

    create.get = ajaxGet;
    create.post = ajaxPost;
    create.delete = ajaxDelete;
    create.put = ajaxPut;
    create.patch = ajaxPatch;
    create.getJSON = ajaxGetJSON;

    return <AjaxCreationMethod>create;
  })();

  private request: AjaxRequest;

  constructor(urlOrRequest: string | AjaxRequest) {
    super();

    const request: AjaxRequest = {
      async: true,
      createXHR: function(this: AjaxRequest) {
        return this.crossDomain ? getCORSRequest.call(this) : getXMLHttpRequest();
      },
      crossDomain: false,
      withCredentials: false,
      headers: {},
      method: 'GET',
      responseType: 'json',
      timeout: 0
    };

    if (typeof urlOrRequest === 'string') {
      request.url = urlOrRequest;
    } else {
      for (const prop in urlOrRequest) {
        if (urlOrRequest.hasOwnProperty(prop)) {
          request[prop] = urlOrRequest[prop];
        }
      }
    }

    this.request = request;
  }

  /** @deprecated internal use only */ _subscribe(subscriber: Subscriber<T>): TeardownLogic {
    return new AjaxSubscriber(subscriber, this.request);
  }
}

/**
 * We need this JSDoc comment for affecting ESDoc.
 * @ignore
 * @extends {Ignored}
 */
export class AjaxSubscriber<T> extends Subscriber<Event> {
  private xhr: XMLHttpRequest;
  private done: boolean = false;

  constructor(destination: Subscriber<T>, public request: AjaxRequest) {
    super(destination);

    const headers = request.headers = request.headers || {};

    // force CORS if requested
    if (!request.crossDomain && !headers['X-Requested-With']) {
      headers['X-Requested-With'] = 'XMLHttpRequest';
    }

    // ensure content type is set
    if (!('Content-Type' in headers) && !(root.FormData && request.body instanceof root.FormData) && typeof request.body !== 'undefined') {
      headers['Content-Type'] = 'application/x-www-form-urlencoded; charset=UTF-8';
    }

    // properly serialize body
    request.body = this.serializeBody(request.body, request.headers['Content-Type']);

    this.send();
  }

  next(e: Event): void {
    this.done = true;
    const { xhr, request, destination } = this;
    const response = new AjaxResponse(e, xhr, request);

    destination.next(response);
  }

  private send(): XMLHttpRequest {
    const {
      request,
      request: { user, method, url, async, password, headers, body }
    } = this;
    const createXHR = request.createXHR;
    const xhr: XMLHttpRequest = tryCatch(createXHR).call(request);

    if (<any>xhr === errorObject) {
      this.error(errorObject.e);
    } else {
      this.xhr = xhr;

      // set up the events before open XHR
      // https://developer.mozilla.org/en/docs/Web/API/XMLHttpRequest/Using_XMLHttpRequest
      // You need to add the event listeners before calling open() on the request.
      // Otherwise the progress events will not fire.
      this.setupEvents(xhr, request);
      // open XHR
      let result: any;
      if (user) {
        result = tryCatch(xhr.open).call(xhr, method, url, async, user, password);
      } else {
        result = tryCatch(xhr.open).call(xhr, method, url, async);
      }

      if (result === errorObject) {
        this.error(errorObject.e);
        return null;
      }

      // timeout, responseType and withCredentials can be set once the XHR is open
      if (async) {
        xhr.timeout = request.timeout;
        xhr.responseType = request.responseType as any;
      }

      if ('withCredentials' in xhr) {
        xhr.withCredentials = !!request.withCredentials;
      }

      // set headers
      this.setHeaders(xhr, headers);

      // finally send the request
      result = body ? tryCatch(xhr.send).call(xhr, body) : tryCatch(xhr.send).call(xhr);
      if (result === errorObject) {
        this.error(errorObject.e);
        return null;
      }
    }

    return xhr;
  }

  private serializeBody(body: any, contentType?: string) {
    if (!body || typeof body === 'string') {
      return body;
    } else if (root.FormData && body instanceof root.FormData) {
      return body;
    }

    if (contentType) {
      const splitIndex = contentType.indexOf(';');
      if (splitIndex !== -1) {
        contentType = contentType.substring(0, splitIndex);
      }
    }

    switch (contentType) {
      case 'application/x-www-form-urlencoded':
        return Object.keys(body).map(key => `${encodeURI(key)}=${encodeURI(body[key])}`).join('&');
      case 'application/json':
        return JSON.stringify(body);
      default:
        return body;
    }
  }

  private setHeaders(xhr: XMLHttpRequest, headers: Object) {
    for (let key in headers) {
      if (headers.hasOwnProperty(key)) {
        xhr.setRequestHeader(key, headers[key]);
      }
    }
  }

  private setupEvents(xhr: XMLHttpRequest, request: AjaxRequest) {
    const progressSubscriber = request.progressSubscriber;

    function xhrTimeout(this: XMLHttpRequest, e: ProgressEvent) {
      const {subscriber, progressSubscriber, request } = (<any>xhrTimeout);
      if (progressSubscriber) {
        progressSubscriber.error(e);
      }
      subscriber.error(new AjaxTimeoutError(this, request)); //TODO: Make betterer.
    };
    xhr.ontimeout = xhrTimeout;
    (<any>xhrTimeout).request = request;
    (<any>xhrTimeout).subscriber = this;
    (<any>xhrTimeout).progressSubscriber = progressSubscriber;
    if (xhr.upload && 'withCredentials' in xhr) {
      if (progressSubscriber) {
        let xhrProgress: (e: ProgressEvent) => void;
        xhrProgress = function(e: ProgressEvent) {
          const { progressSubscriber } = (<any>xhrProgress);
          progressSubscriber.next(e);
        };
        if (root.XDomainRequest) {
          xhr.onprogress = xhrProgress;
        } else {
          xhr.upload.onprogress = xhrProgress;
        }
        (<any>xhrProgress).progressSubscriber = progressSubscriber;
      }
      let xhrError: (e: ErrorEvent) => void;
      xhrError = function(this: XMLHttpRequest, e: ErrorEvent) {
        const { progressSubscriber, subscriber, request } = (<any>xhrError);
        if (progressSubscriber) {
          progressSubscriber.error(e);
        }
        subscriber.error(new AjaxError('ajax error', this, request));
      };
      xhr.onerror = xhrError;
      (<any>xhrError).request = request;
      (<any>xhrError).subscriber = this;
      (<any>xhrError).progressSubscriber = progressSubscriber;
    }

    function xhrReadyStateChange(this: XMLHttpRequest, e: ProgressEvent) {
      const { subscriber, progressSubscriber, request } = (<any>xhrReadyStateChange);
      if (this.readyState === 4) {
        // normalize IE9 bug (http://bugs.jquery.com/ticket/1450)
        let status: number = this.status === 1223 ? 204 : this.status;
        let response: any = (this.responseType === 'text' ?  (
          this.response || this.responseText) : this.response);

        // fix status code when it is 0 (0 status is undocumented).
        // Occurs when accessing file resources or on Android 4.1 stock browser
        // while retrieving files from application cache.
        if (status === 0) {
          status = response ? 200 : 0;
        }

        if (200 <= status && status < 300) {
          if (progressSubscriber) {
            progressSubscriber.complete();
          }
          subscriber.next(e);
          subscriber.complete();
        } else {
          if (progressSubscriber) {
            progressSubscriber.error(e);
          }
          subscriber.error(new AjaxError('ajax error ' + status, this, request));
        }
      }
    };
    xhr.onreadystatechange = xhrReadyStateChange;
    (<any>xhrReadyStateChange).subscriber = this;
    (<any>xhrReadyStateChange).progressSubscriber = progressSubscriber;
    (<any>xhrReadyStateChange).request = request;
  }

  unsubscribe() {
    const { done, xhr } = this;
    if (!done && xhr && xhr.readyState !== 4 && typeof xhr.abort === 'function') {
      xhr.abort();
    }
    super.unsubscribe();
  }
}

/**
 * A normalized AJAX response.
 *
 * @see {@link ajax}
 *
 * @class AjaxResponse
 */
export class AjaxResponse {
  /** @type {number} The HTTP status code */
  status: number;

  /** @type {string|ArrayBuffer|Document|object|any} The response data */
  response: any;

  /** @type {string} The raw responseText */
  responseText: string;

  /** @type {string} The responseType (e.g. 'json', 'arraybuffer', or 'xml') */
  responseType: string;

  constructor(public originalEvent: Event, public xhr: XMLHttpRequest, public request: AjaxRequest) {
    this.status = xhr.status;
    this.responseType = xhr.responseType || request.responseType;
    this.response = parseXhrResponse(this.responseType, xhr);
  }
}

/**
 * A normalized AJAX error.
 *
 * @see {@link ajax}
 *
 * @class AjaxError
 */
export class AjaxError extends Error {
  /** @type {XMLHttpRequest} The XHR instance associated with the error */
  xhr: XMLHttpRequest;

  /** @type {AjaxRequest} The AjaxRequest associated with the error */
  request: AjaxRequest;

  /** @type {number} The HTTP status code */
  status: number;

  /** @type {string} The responseType (e.g. 'json', 'arraybuffer', or 'xml') */
  responseType: string;

  /** @type {string|ArrayBuffer|Document|object|any} The response data */
  response: any;

  constructor(message: string, xhr: XMLHttpRequest, request: AjaxRequest) {
    super(message);
    this.message = message;
    this.xhr = xhr;
    this.request = request;
    this.status = xhr.status;
    this.responseType = xhr.responseType || request.responseType;
    this.response = parseXhrResponse(this.responseType, xhr);
  }
}

function parseXhrResponse(responseType: string, xhr: XMLHttpRequest) {
  switch (responseType) {
    case 'json':
        if ('response' in xhr) {
          //IE does not support json as responseType, parse it internally
          return xhr.responseType ? xhr.response : JSON.parse(xhr.response || xhr.responseText || 'null');
        } else {
          // HACK(benlesh): TypeScript shennanigans
          // tslint:disable-next-line:no-any latest TS seems to think xhr is "never" here.
          return JSON.parse((xhr as any).responseText || 'null');
        }
      case 'xml':
        return xhr.responseXML;
      case 'text':
      default:
          // HACK(benlesh): TypeScript shennanigans
          // tslint:disable-next-line:no-any latest TS seems to think xhr is "never" here.
          return  ('response' in xhr) ? xhr.response : (xhr as any).responseText;
  }
}

/**
 * @see {@link ajax}
 *
 * @class AjaxTimeoutError
 */
export class AjaxTimeoutError extends AjaxError {
  constructor(xhr: XMLHttpRequest, request: AjaxRequest) {
    super('ajax timeout', xhr, request);
  }
}
