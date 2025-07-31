"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.DefaultFetcher = exports.BaseFetcher = void 0;
const debug_1 = __importDefault(require("debug"));
const fs_1 = __importDefault(require("fs"));
const make_fetch_happen_1 = __importDefault(require("make-fetch-happen"));
const util_1 = __importDefault(require("util"));
const error_1 = require("./error");
const tmpfile_1 = require("./utils/tmpfile");
const log = (0, debug_1.default)('tuf:fetch');
class BaseFetcher {
    // Download file from given URL. The file is downloaded to a temporary
    // location and then passed to the given handler. The handler is responsible
    // for moving the file to its final location. The temporary file is deleted
    // after the handler returns.
    async downloadFile(url, maxLength, handler) {
        return (0, tmpfile_1.withTempFile)(async (tmpFile) => {
            const reader = await this.fetch(url);
            let numberOfBytesReceived = 0;
            const fileStream = fs_1.default.createWriteStream(tmpFile);
            // Read the stream a chunk at a time so that we can check
            // the length of the file as we go
            try {
                for await (const chunk of reader) {
                    numberOfBytesReceived += chunk.length;
                    if (numberOfBytesReceived > maxLength) {
                        throw new error_1.DownloadLengthMismatchError('Max length reached');
                    }
                    await writeBufferToStream(fileStream, chunk);
                }
            }
            finally {
                // Make sure we always close the stream
                // eslint-disable-next-line @typescript-eslint/unbound-method
                await util_1.default.promisify(fileStream.close).bind(fileStream)();
            }
            return handler(tmpFile);
        });
    }
    // Download bytes from given URL.
    async downloadBytes(url, maxLength) {
        return this.downloadFile(url, maxLength, async (file) => {
            const stream = fs_1.default.createReadStream(file);
            const chunks = [];
            for await (const chunk of stream) {
                chunks.push(chunk);
            }
            return Buffer.concat(chunks);
        });
    }
}
exports.BaseFetcher = BaseFetcher;
class DefaultFetcher extends BaseFetcher {
    timeout;
    retry;
    constructor(options = {}) {
        super();
        this.timeout = options.timeout;
        this.retry = options.retry;
    }
    async fetch(url) {
        log('GET %s', url);
        const response = await (0, make_fetch_happen_1.default)(url, {
            timeout: this.timeout,
            retry: this.retry,
        });
        if (!response.ok || !response?.body) {
            throw new error_1.DownloadHTTPError('Failed to download', response.status);
        }
        return response.body;
    }
}
exports.DefaultFetcher = DefaultFetcher;
const writeBufferToStream = async (stream, buffer) => {
    return new Promise((resolve, reject) => {
        stream.write(buffer, (err) => {
            if (err) {
                reject(err);
            }
            resolve(true);
        });
    });
};
