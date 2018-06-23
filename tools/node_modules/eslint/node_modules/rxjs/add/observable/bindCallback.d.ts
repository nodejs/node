import { bindCallback as staticBindCallback } from '../../observable/bindCallback';
declare module '../../Observable' {
    namespace Observable {
        let bindCallback: typeof staticBindCallback;
    }
}
