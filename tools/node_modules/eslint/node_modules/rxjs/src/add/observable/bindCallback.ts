import { Observable } from '../../Observable';
import { bindCallback as staticBindCallback } from '../../observable/bindCallback';

Observable.bindCallback = staticBindCallback;

declare module '../../Observable' {
  namespace Observable {
    export let bindCallback: typeof staticBindCallback;
  }
}
