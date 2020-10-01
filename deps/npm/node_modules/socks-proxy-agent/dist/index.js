"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
const agent_1 = __importDefault(require("./agent"));
function createSocksProxyAgent(opts) {
    return new agent_1.default(opts);
}
(function (createSocksProxyAgent) {
    createSocksProxyAgent.SocksProxyAgent = agent_1.default;
    createSocksProxyAgent.prototype = agent_1.default.prototype;
})(createSocksProxyAgent || (createSocksProxyAgent = {}));
module.exports = createSocksProxyAgent;
//# sourceMappingURL=index.js.map