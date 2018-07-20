import { root } from './root';
import { MapPolyfill } from './MapPolyfill';
export const Map = root.Map || (() => MapPolyfill)();
//# sourceMappingURL=Map.js.map