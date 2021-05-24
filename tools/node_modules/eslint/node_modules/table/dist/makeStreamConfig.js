"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.makeStreamConfig = void 0;
const lodash_clonedeep_1 = __importDefault(require("lodash.clonedeep"));
const utils_1 = require("./utils");
const validateConfig_1 = require("./validateConfig");
/**
 * Creates a configuration for every column using default
 * values for the missing configuration properties.
 */
const makeColumnsConfig = (columnCount, columns = {}, columnDefault) => {
    return Array.from({ length: columnCount }).map((_, index) => {
        return {
            alignment: 'left',
            paddingLeft: 1,
            paddingRight: 1,
            truncate: Number.POSITIVE_INFINITY,
            verticalAlignment: 'top',
            wrapWord: false,
            ...columnDefault,
            ...columns[index],
        };
    });
};
/**
 * Makes a new configuration object out of the userConfig object
 * using default values for the missing configuration properties.
 */
const makeStreamConfig = (userConfig) => {
    validateConfig_1.validateConfig('streamConfig.json', userConfig);
    const config = lodash_clonedeep_1.default(userConfig);
    if (config.columnDefault.width === undefined) {
        throw new Error('Must provide config.columnDefault.width when creating a stream.');
    }
    return {
        drawVerticalLine: () => {
            return true;
        },
        ...config,
        border: utils_1.makeBorderConfig(config.border),
        columns: makeColumnsConfig(config.columnCount, config.columns, config.columnDefault),
    };
};
exports.makeStreamConfig = makeStreamConfig;
