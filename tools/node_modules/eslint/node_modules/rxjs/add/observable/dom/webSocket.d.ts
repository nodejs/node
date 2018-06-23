import { webSocket as staticWebSocket } from '../../../observable/dom/webSocket';
declare module '../../../Observable' {
    namespace Observable {
        let webSocket: typeof staticWebSocket;
    }
}
