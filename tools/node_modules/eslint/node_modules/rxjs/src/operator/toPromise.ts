import { Observable } from '../Observable';

// HACK: this is here for backward compatability
// TODO(benlesh): remove this in v6.
export const toPromise: typeof Observable.prototype.toPromise = Observable.prototype.toPromise;
