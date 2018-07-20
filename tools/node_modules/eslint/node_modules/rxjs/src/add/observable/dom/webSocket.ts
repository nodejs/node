import { Observable } from '../../../Observable';
import { webSocket as staticWebSocket } from '../../../observable/dom/webSocket';

Observable.webSocket = staticWebSocket;

declare module '../../../Observable' {
  namespace Observable {
    export let webSocket: typeof staticWebSocket;
  }
}