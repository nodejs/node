import { Observable } from '../../Observable';
import { bindNodeCallback as staticBindNodeCallback } from '../../observable/bindNodeCallback';

Observable.bindNodeCallback = staticBindNodeCallback;

declare module '../../Observable' {
  namespace Observable {
    export let bindNodeCallback: typeof staticBindNodeCallback;
  }
}