import { bindNodeCallback as staticBindNodeCallback } from '../../observable/bindNodeCallback';
declare module '../../Observable' {
    namespace Observable {
        let bindNodeCallback: typeof staticBindNodeCallback;
    }
}
