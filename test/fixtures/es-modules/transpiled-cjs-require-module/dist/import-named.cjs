"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
var logger_1 = require("logger");
new logger_1.JSONConsumer().attach();
var logger = logger_1.createLogger();
logger.info('import named');
