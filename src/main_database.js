//this is a design for the DBMS and detabase
//use GPL3.0 lisense
const fs = require('fs');
const readline = require('readline');
const path = require('path');
const zlib = require('zlib');
//const csv = require('csv-parser');
const crypto = require('crypto');
const util = require('util');
const gzipAsync = util.promisify(zlib.gzip);
// 在文件顶部添加依赖
const Database = require('node:sqlite');
// 在文件顶部添加自定义 Queue 类，替代 require('queue')
class Queue {
    constructor(options = {}) {
        this.items = [];
        this.concurrency = options.concurrency || 1;
        this.running = 0;
        this.autostart = options.autostart || false;
        this.eventListeners = {};
    }

    // 添加任务到队列
    push(task) {
        this.items.push(task);
        if (this.autostart && this.running < this.concurrency) {
            this._processNext();
        }
        return this;
    }

    // 获取队列长度
    get length() {
        return this.items.length;
    }

    // 处理下一个任务
    _processNext() {
        if (this.running >= this.concurrency || this.items.length === 0) {
            return;
        }

        const task = this.items.shift();
        this.running++;

        Promise.resolve()
            .then(() => task())
            .catch(err => {
                console.error('Queue task error:', err);
            })
            .finally(() => {
                this.running--;
                // 如果队列中还有任务，继续处理
                if (this.items.length > 0) {
                    this._processNext();
                } else if (this.running === 0) {
                    // 所有任务完成，触发 end 事件
                    this._emit('end');
                }
            });

        // 如果仍有并发空间，继续处理下一个任务
        if (this.running < this.concurrency) {
            this._processNext();
        }
    }

    // 添加事件监听器
    once(event, callback) {
        if (!this.eventListeners[event]) {
            this.eventListeners[event] = [];
        }
        this.eventListeners[event].push({ callback, once: true });
        return this;
    }

    // 触发事件
    _emit(event, ...args) {
        const listeners = this.eventListeners[event];
        if (!listeners) return;

        // 执行所有监听器并移除一次性监听器
        for (let i = listeners.length - 1; i >= 0; i--) {
            const listener = listeners[i];
            listener.callback(...args);
            if (listener.once) {
                listeners.splice(i, 1);
            }
        }
    }
}

// 删除外部依赖
// const { Queue } = require('queue');  // 删除这一行

const rl = readline.createInterface({
    input: process.stdin,
    output: process.stdout
});

// 添加日志函数
function log(level, message) {
    const timestamp = new Date().toISOString();
    const logMessage = `${timestamp} [${level}] ${message}`;
    console.log(logMessage);
    if (level !== 'DEBUG') { // 只记录非 DEBUG 级别的日志到文件
        fs.appendFileSync('dbms.log', logMessage + '\n');
    }
}

// 错误处理函数
function handleError(error, operation) {
    log('ERROR', `Error during ${operation}: ${error.message}`);
    console.error(`An error occurred during ${operation}. Please check the logs for more details.`);
}

const { LRUCache } = require('lru-cache');
class PartitionedTable {

    constructor(tableName, partitionKey, numPartitions = 10) {
        this.tableName = tableName;
        this.partitionKey = partitionKey;
        this.numPartitions = numPartitions;
        this.partitions = Array.from({ length: numPartitions }, () => new Map());
        this.compressed = false;
        this.compressionEnabled = false;
        this.riemannSphereDB = new RiemannSphereDB(); // 添加这一行
        this._verifyPartitions();
        //console.log(`Initialized partitions for table ${tableName}:`, this.partitions);
    }

    _verifyPartitions() {
        if (!Array.isArray(this.partitions) || this.partitions.length !== this.numPartitions) {
            throw new Error('Invalid partitions array');
        }
    }



    // 根据分区键计算分区索引
    getPartitionIndex(data) {
        const keyValue = String(data[this.partitionKey]); // 强制转字符串
        const hash = this.hash(keyValue);
        const partitionIndex = hash % this.numPartitions;
        return partitionIndex;
    }

    // 简单的哈希函数
    hash(key) {
        let hash = 0;
        for (let i = 0; i < key.length; i++) {
            hash = (hash << 5) - hash + key.charCodeAt(i);
            hash |= 0; // 转换为 32 位整数
        }
        const absHash = Math.abs(hash);
        //console.log(`Hash for key ${key}: ${absHash}`); // 添加调试信息
        return absHash;
    }

    // 插入数据
    insert(data) {
        const partitionIndex = this.getPartitionIndex(data);
        const partition = this.partitions[partitionIndex];
        const key = String(data[this.partitionKey]); // 强制转字符串
        if (!partition) {
            this.partitions[partitionIndex] = new Map();
        }
        this.partitions[partitionIndex].set(key, data);
    }

    // PartitionedTable 类中的 query 方法
    // PartitionedTable 类中的 query 方法
    query(query, fields = ['*']) {
        const pk = this.partitionKey;
        // 主键精确匹配，直接定位分区和key
        if (
            query &&
            query[pk] &&
            typeof query[pk] === 'object' &&
            query[pk]._op === '='
        ) {
            const key = String(query[pk]._value);
            const partitionIndex = this.getPartitionIndex({ [pk]: key });
            const partition = this.partitions[partitionIndex];
            if (partition && partition.has(key)) {
                const data = partition.get(key);
                if (fields && fields.length && fields[0] !== '*') {
                    const filtered = {};
                    fields.forEach(f => {
                        if (data.hasOwnProperty(f)) filtered[f] = data[f];
                    });
                    return [filtered];
                } else {
                    return [{ ...data }];
                }
            }
            return [];
        }
        // 其他情况仍然遍历所有分区
        const results = [];
        this.partitions.forEach(partition => {
            if (partition) {
                for (const [key, data] of partition.entries()) {
                    if (data && typeof data === 'object' && !Array.isArray(data)) {
                        if (this.isMatch(data, query || {})) {
                            if (fields && fields.length && fields[0] !== '*') {
                                const filtered = {};
                                fields.forEach(f => {
                                    if (data.hasOwnProperty(f)) filtered[f] = data[f];
                                });
                                results.push(filtered);
                            } else {
                                results.push({ ...data });
                            }
                        }
                    }
                }
            }
        });
        return results.filter(Boolean);
    }

    isMatch(data, query) {
        try {
            // 处理空查询
            if (!query || Object.keys(query).length === 0) {
                return true;
            }

            // 处理逻辑运算符
            if (query._op) {
                switch (query._op.toLowerCase()) {
                    case 'and':
                        return query._conditions.every(condition => this.isMatch(data, condition));
                    case 'or':
                        return query._conditions.some(condition => this.isMatch(data, condition));
                    case 'not':
                        return !this.isMatch(data, query._condition);
                    default:
                        log('ERROR', `未知的逻辑运算符: ${query._op}`);
                        return false;
                }
            }

            // 处理字段条件
            return Object.entries(query).every(([field, condition]) => {
                if (!data.hasOwnProperty(field)) {
                    //log('DEBUG', `字段 ${field} 未在数据中找到`);
                    return false;
                }

                const fieldValue = data[field];
                //log('DEBUG', `检查字段 ${field}=${fieldValue}`);

                // 处理子查询
                if (condition.column && condition.subquery) {
                    try {
                        const subQueryResults = this._executeSubQuery(condition.subquery);
                        if (!subQueryResults || subQueryResults.length === 0) {
                            return false;
                        }

                        // 从子查询结果中获取匹配值
                        const targetValue = subQueryResults[0][condition.column];

                        // 调试输出
                        //log('DEBUG', `比较: ${field}=${fieldValue} 与子查询结果 ${condition.column}=${targetValue}`);

                        // 将两个值都转换为数字进行比较
                        const numFieldValue = Number(fieldValue);
                        const numTargetValue = Number(targetValue);

                        // 如果两个值都能转换为有效数字，进行数值比较
                        if (!isNaN(numFieldValue) && !isNaN(numTargetValue)) {
                            //log('DEBUG', `数值比较: ${numFieldValue} === ${numTargetValue}`);
                            return numFieldValue === numTargetValue;
                        }

                        // 否则进行字符串比较
                        //log('DEBUG', `字符串比较: ${String(fieldValue)} === ${String(targetValue)}`);
                        return String(fieldValue) === String(targetValue);

                    } catch (error) {
                        log('ERROR', `子查询执行失败: ${error.message}`);
                        return false;
                    }
                }


                // 处理比较运算符
                if (typeof condition === 'object' && condition._op) {
                    const compareValue = condition._value;
                    const numFieldValue = Number.isNaN(Number(fieldValue)) ? fieldValue : Number(fieldValue);
                    const numCompareValue = Number.isNaN(Number(compareValue)) ? compareValue : Number(compareValue);

                    switch (condition._op) {
                        case '=': return numFieldValue === numCompareValue;
                        case '>': return numFieldValue > numCompareValue;
                        case '<': return numFieldValue < numCompareValue;
                        case '>=': return numFieldValue >= numCompareValue;
                        case '<=': return numFieldValue <= numCompareValue;
                        case '<>': return numFieldValue !== numCompareValue;
                        default: return false;
                    }
                }

                // 直接值比较
                return fieldValue === condition;
            });
        } catch (error) {
            log('ERROR', `匹配检查失败: ${error.message}`);
            return false;
        }
    }


    _executeSubQuery(subquery) {
        try {
            // 解析子查询
            const tokens = subquery.trim().toLowerCase().split(/\s+/);
            const selectIndex = tokens.indexOf('select');
            const fromIndex = tokens.indexOf('from');
            const whereIndex = tokens.indexOf('where');

            // 验证子查询格式
            if (selectIndex === -1 || fromIndex === -1 || selectIndex > fromIndex) {
                throw new Error('Invalid subquery format');
            }

            // 解析目标列名
            const targetColumns = tokens.slice(selectIndex + 1, fromIndex);
            if (targetColumns.length === 0) {
                throw new Error('No columns specified in subquery');
            }

            // 解析表名
            const tableName = tokens[fromIndex + 1];
            if (!tableName) {
                throw new Error('No table specified in subquery');
            }

            // 获取当前数据库
            if (!global.hypercubeDB) {
                throw new Error('Database instance not found');
            }

            const currentDB = global.hypercubeDB.getCurrentDatabase();
            if (!currentDB) {
                throw new Error('No database selected');
            }

            // 获取目标表
            const db = global.hypercubeDB.data.get(global.hypercubeDB.currentDatabase);
            if (!db) {
                throw new Error('Current database not found');
            }

            const targetTable = db.get(tableName);
            if (!targetTable) {
                throw new Error(`Table ${tableName} not found`);
            }

            // 构建查询条件
            let conditions = {};
            if (whereIndex !== -1) {
                const whereClause = tokens.slice(whereIndex + 1).join(' ');
                conditions = parseConditions(whereClause);
            }

            // 执行查询并只返回指定列
            const results = targetTable.query(conditions)
                .map(row => {
                    const selectedColumns = {};
                    targetColumns.forEach(col => {
                        if (row.hasOwnProperty(col)) {
                            selectedColumns[col] = row[col];
                        }
                    });
                    return selectedColumns;
                })
                .filter(row => Object.keys(row).length > 0); // 过滤掉空结果

            if (results.length === 0) {
                //log('DEBUG', '子查询没有找到匹配的结果');
                return [];
            }

            //log('DEBUG', `子查询执行成功，找到 ${results.length} 条结果，列: ${targetColumns.join(', ')}`);
            return results;

            //log('DEBUG', `Subquery executed successfully with ${results.length} results for ${targetColumns.join(', ')}`);
            //return results;

        } catch (error) {
            log('ERROR', `子查询执行失败: ${error.message}`);
            throw error;
        }
    }



    getRawKey(processedKey) {
        // 逆向处理压缩后的键值
        if (this.compressed) {
            return this.reverseKeyProcessing(processedKey);
        }
        return processedKey;
    }

    reverseKeyProcessing(key) {
        // 实现键值逆向处理逻辑（根据实际压缩算法实现）
        try {
            const crypto = require('crypto');
            return crypto.createHash('sha256').update(String(key)).digest('hex');
        } catch (e) {
            return key; // 回退逻辑
        }
    }
}


class HypercubeDB {
    constructor({
        partitionFactor = 8,
        cacheStrategy = 'dimensional',
        vectorizationThreshold = 50
    } = {}) {
        this.optimizationParams = {
            PARTITION_FACTOR: partitionFactor,
            CACHE_STRATEGY: cacheStrategy,
            VECTORIZATION_THRESHOLD: vectorizationThreshold
        };
        this.riemannSphereDB = new RiemannSphereDB();
        this.data = new Map();
        this.indexes = new Map();
        this.locks = new Map();
        this.distanceHashes = new Map();
        this.queryCache = new Map(); // 查询缓存
        this.loadDataAsync();
        this.pyramidIndex = null;
        this.recursiveSphereWeaving = null;
        this._applyOptimizationConfig();
        this.isLoaded = false; // 添加标志位
        this.extensions = new Map();

        this.isDroppingDatabase = false;
        this.dimensiontree = null;
        this.mounts = new Map(); // dbName -> 外部路径
        this.saveQueue = new Queue({ concurrency: 1, autostart: true });
        this.sqliteDBs = new Map(); // dbName -> SQLite连接
        this.pendingSaves = new Set(); // 跟踪待保存的数据库

    }
    async mount(dbName, externalPath) {
        this.mounts.set(dbName, externalPath);
        // 只加载该db
        await this._loadSingleDatabase(dbName, externalPath);
    }
    // 只加载单个数据库（支持本地和挂载路径）
// ...existing code...
// 只加载单个数据库（支持本地和挂载路径，自动识别SQLite/JSON）
async _loadSingleDatabase(dbName, filePath) {
    try {
        const absPath = path.resolve(filePath);
        if (!fs.existsSync(absPath)) {
            log('ERROR', `Database file not found: ${absPath}`);
            return;
        }
        // 判断文件类型
        if (absPath.endsWith('.db')) {
            // SQLite方式加载
            const sqliteDB = new Database(absPath);
            this.sqliteDBs.set(dbName, sqliteDB);
            const tables = await sqliteDB.prepare("SELECT name FROM sqlite_master WHERE type='table'").all();
            const partitionedTables = new Map();
            for (const { name: tableName } of tables) {
                // 自动分页读取数据
                const pageSize = 1000;
                let offset = 0;
                let allRows = [];
                while (true) {
                    const rows =await sqliteDB.prepare(`SELECT * FROM ${tableName} LIMIT ? OFFSET ?`).all(pageSize, offset);
                    if (rows.length === 0) break;
                    allRows.push(...rows);
                    offset += pageSize;
                }
                // 恢复为 PartitionedTable 实例
                // 自动推断分区键和分区数（可优化为从表名或约定字段获取）
                const partitionKey = 'id'; // 可根据实际表结构调整
                const numPartitions = 10;  // 可根据实际表结构调整
                const partitionedTable = new PartitionedTable(tableName, partitionKey, numPartitions);
                for (const row of allRows) {
                    partitionedTable.insert(row);
                }
                partitionedTables.set(tableName, partitionedTable);
            }
            this.data.set(dbName, partitionedTables);
            log('INFO', `Database ${dbName} loaded from SQLite file ${absPath}`);
        } else {
            // JSON方式加载
            const rawData = await fs.promises.readFile(absPath);
            const dbData = JSON.parse(rawData.toString());
            const partitionedTables = new Map();

            for (const [tableName, tableData] of Object.entries(dbData)) {
                if (!tableData.partitionKey || !Array.isArray(tableData.partitions)) {
                    log('ERROR', `Invalid table structure for ${tableName}`);
                    continue;
                }
                const partitionedTable = new PartitionedTable(tableName, tableData.partitionKey, tableData.numPartitions || 10);
                partitionedTable.compressed = !!tableData.compressed;
                partitionedTable.partitions = tableData.partitions.map(partition => {
                    if (!Array.isArray(partition)) return new Map();
                    const validPartition = new Map();
                    for (const entry of partition) {
                        if (!Array.isArray(entry) || entry.length !== 2) continue;
                        const [key, value] = entry;
                        if (key === undefined || value === null) continue;
                        try {
                            if (partitionedTable.compressed && value?.compressed) {
                                const decompressed = this.conicalProjectionDecompress(value.compressed);
                                validPartition.set(key, decompressed);
                            } else {
                                validPartition.set(key, value);
                            }
                        } catch (e) {
                            log('ERROR', `Error processing ${key}: ${e.message}`);
                            validPartition.set(key, value);
                        }
                    }
                    return validPartition;
                });
                partitionedTables.set(tableName, partitionedTable);
            }
            this.data.set(dbName, partitionedTables);
            log('INFO', `Database ${dbName} loaded from ${absPath}`);
        }
    } catch (error) {
        log('ERROR', `Failed to load database ${dbName}: ${error.message}`);
    }
}
    // 只保存单个数据库（支持本地和挂载路径）
    // ...existing code...
    // 只保存单个数据库（支持本地和挂载路径），异步队列+SQLite
async _saveSingleDatabase(dbName, filePath) {
    try {
        if (!this.data.has(dbName)) {
            log('INFO', `No data to save for database: ${dbName}`);
            return;
        }
        let sqliteDB = this.sqliteDBs.get(dbName);
        if (!sqliteDB) {
            sqliteDB = new Database(path.resolve(filePath));
            this.sqliteDBs.set(dbName, sqliteDB);
        }
        await sqliteDB.exec('BEGIN TRANSACTION');
        try {
            const tables = this.data.get(dbName);
            for (const [tableName, tableObj] of tables.entries()) {
                if (!(tableObj instanceof PartitionedTable)) continue;
                let sample = null;
                for (const partition of tableObj.partitions) {
                    if (partition && partition.size > 0) {
                        sample = partition.values().next().value;
                        break;
                    }
                }
                if (!sample) continue;
                const pk = tableObj.partitionKey || 'id';
                const columnsArr = Object.keys(sample).filter(col => col !== pk);
                const columns = columnsArr.map(col => `${col} TEXT`).join(',');
               await sqliteDB.exec(`CREATE TABLE IF NOT EXISTS ${tableName} (${columns}, ${pk} TEXT PRIMARY KEY)`);
               await sqliteDB.exec(`DELETE FROM ${tableName}`);
                const insertStmt = sqliteDB.prepare(
                    `INSERT INTO ${tableName} (${[...columnsArr, pk].join(',')}) VALUES (${[...columnsArr.map(() => '?'), '?'].join(',')})`
                );
                let batch = [];
                for (const partition of tableObj.partitions) {
                    if (!partition) continue;
                    for (const [id, row] of partition.entries()) {
                        batch.push([...columnsArr.map(col => row[col]), id]);
                        if (batch.length >= 1000) {
                            for (const params of batch) insertStmt.run(...params);
                            batch = [];
                        }
                    }
                }
                for (const params of batch) insertStmt.run(...params);
            }
            await sqliteDB.exec('COMMIT');
            log('INFO', `Database ${dbName} saved to SQLite file ${filePath}`);
        } catch (txError) {
           await sqliteDB.exec('ROLLBACK');
            log('ERROR', `Transaction failed for ${dbName}: ${txError.message}`);
        }
    } catch (error) {
        log('ERROR', `Save operation failed for ${dbName}: ${error.message}`);
    }
}
    async demount(dbName) {
        if (!this.mounts.has(dbName)) return;
        const externalPath = this.mounts.get(dbName);
        // 只保存该db
        await this._saveSingleDatabase(dbName, externalPath);
        this.data.delete(dbName);
        this.mounts.delete(dbName);
    }

    getCurrentDatabase() {
        return this.currentDatabase;
    }
    // 应用优化配置
    _applyOptimizationConfig() {
        this.partitionFactor = this.optimizationParams.PARTITION_FACTOR;
        this.cache = this.optimizationParams.CACHE_STRATEGY === 'dimensional' ?
            new LRUCache({ max: 5000 }) : null;
    }

    // 有损压缩方法（集成黎曼球面映射）
    conicalProjectionCompress(dataPoint, focalPoint = [0, 0, -Infinity]) {
        // 使用RiemannSphereDB进行空间映射
        // 添加数据验证
        if (!dataPoint || typeof dataPoint !== 'object') {
            log('ERROR', 'Invalid data point for compression');
            return [Date.now() % 1000, 0, 0];
        }
        const mappedPoint = this.riemannSphereDB.mapToRiemannSphere(dataPoint);
        // 检查是否禁用压缩
        if (this.compressionDisabled) {
            return dataPoint;
        }

        // 强化输入验证
        if (!dataPoint || typeof dataPoint !== 'object') {
            log('ERROR', 'Invalid data point for compression');
            return [Date.now() % 1000, 0, 0];
        }

        // 统一数据格式处理
        let dpArray;
        try {
            // 确保crypto模块可用

            if (!crypto || !crypto.createHash) {
                throw new Error('Crypto module not available');
            }

            if (Array.isArray(dataPoint)) {
                dpArray = dataPoint.filter(v => v !== undefined && v !== null);
            } else {
                const values = Object.values(dataPoint);
                dpArray = values.length > 0 ? values : [dataPoint];
            }

            // 空数据保护
            if (dpArray.length === 0 || dpArray.some(v => v === undefined)) {
                //log('WARN', 'Empty or invalid data point during compression');
                return [Date.now() % 1000, 0, 0];
            }

            // 数值转换和哈希处理
            dpArray = dpArray.map(v => {
                if (typeof v === 'string') {
                    return parseInt(crypto.createHash('md5').update(v).digest('hex'), 16) % 1000;
                }
                const num = Number(v);
                return isNaN(num) ? 0 : num;
            });

            // 确保数据点有足够维度
            while (dpArray.length < 3) {
                dpArray.push(0);
            }

            // 映射到黎曼球面
            const spherePoint = this.riemannSphereDB.mapToRiemannSphere(dpArray);
            if (!spherePoint || spherePoint.some(isNaN)) {
                log('ERROR', 'Invalid sphere mapping result');
                return [Date.now() % 1000, 0, 0];
            }

            // 计算向量和角度
            const toPoint = spherePoint.map((v, i) => v - this.sphereCenter[i]);
            const baseAxis = focalPoint.map((v, i) => v - this.sphereCenter[i]);

            const theta = this.calculateAngle(baseAxis, toPoint);
            const phi = Math.atan2(toPoint[1], toPoint[0]);

            // 返回压缩结果
            return [
                Math.round(Math.tan(theta) * 1e4) / 1e4,
                Math.round(phi * 180 / Math.PI),
                this.calculateIntensity(spherePoint),
                Date.now() % 1000
            ];

        } catch (error) {
            log('ERROR', `Compression failed: ${error.message}`);
            return [Date.now() % 1000, 0, 0];
        }
    }



    // 解压缩方法
    conicalProjectionDecompress(compressedData) {
        // 反量化参数
        const theta = Math.atan(compressedData[0]);
        const phi = compressedData[1] * Math.PI / 180;
        const intensity = compressedData[2];

        // 重建球面坐标
        const x = intensity * Math.sin(theta) * Math.cos(phi);
        const y = intensity * Math.sin(theta) * Math.sin(phi);
        const z = intensity * Math.cos(theta);

        // 反投影到原始数据空间
        return this.inverseRiemannMapping([x, y, z]);
    }

    // 辅助方法
    calculateAngle(v1, v2) {
        const dot = v1.reduce((sum, val, i) => sum + val * v2[i], 0);
        const mag1 = Math.sqrt(v1.reduce((sum, val) => sum + val ** 2, 0));
        const mag2 = Math.sqrt(v2.reduce((sum, val) => sum + val ** 2, 0));
        return Math.acos(dot / (mag1 * mag2));
    }

    calculateIntensity(point) {
        return Math.sqrt(point.reduce((sum, val) => sum + val ** 2, 0));
    }

    // 反投影到原始数据空间
    inverseRiemannMapping(spherePoint) {
        const [x, y, z] = spherePoint;
        const magnitude = Math.sqrt(x ** 2 + y ** 2 + z ** 2);
        const originalPoint = [x / magnitude, y / magnitude, z / magnitude];
        return originalPoint;
    }
    // 修复 loadExtension 方法
    loadExtension(extensionPath) {
        try {
            const fullPath = path.resolve(extensionPath);
            if (!fs.existsSync(fullPath)) {
                console.log("Extension file not found");
                return;
            }

            // 清除模块缓存，确保重新加载
            delete require.cache[fullPath];
            const extension = require(fullPath);

            // 获取扩展名
            const extName = path.basename(extensionPath, '.js').toLowerCase();

            // 修复：检查 execute 是否为对象而不是函数
            if (extension && extension.execute && typeof extension.execute === 'object') {
                this.extensions.set(extName, extension);
                log('INFO', `Extension '${extName}' registered`);
                console.log(`成功加载扩展: ${extName}`);
            } else {
                console.log(`Invalid extension format: ${extensionPath}`);
                console.log(`Extension should have an 'execute' object with methods`);
            }
        } catch (error) {
            console.log(`Failed to load extension: ${error.message}`);
            handleError(error, 'loading extension');
        }
    }

    // 修复 removeExtension 方法 - 统一使用小写
    removeExtension(name) {
        const extKey = name.toLowerCase();
        if (this.extensions.has(extKey)) {
            this.extensions.delete(extKey);
            console.log(`Extension '${name}' removed`);
        } else {
            console.log(`Extension '${name}' not found`);
        }
    }

    // 修复 showExtensions 方法
    showExtensions() {
        if (this.extensions.size > 0) {
            console.log('Loaded extensions:');
            this.extensions.forEach((extension, name) => {
                const methods = extension.execute ? Object.keys(extension.execute) : [];
                console.log(`- ${name}: [${methods.join(', ')}]`);
            });
        } else {
            console.log('No extensions found');
        }
    }
    executeExtension(extensionName, method, ...args) {
        // 统一扩展名为小写
        const extKey = extensionName.toLowerCase();
        if (!this.extensions.has(extKey)) {
            console.log(`Extension '${extensionName}' not found`);
            log('ERROR', `Extension '${extensionName}' not found`);
            this.showExtensions();
            return;
        }
        const extension = this.extensions.get(extKey);

        // 方法名统一小写查找
        const methodKey = method ? method.toLowerCase() : null;
        if (!methodKey || !extension.execute || typeof extension.execute[methodKey] !== 'function') {
            console.log(`Method '${method}' not found in extension '${extensionName}'`);
            log('ERROR', `Method '${method}' not found in extension '${extensionName}'`);
            return;
        }

        try {
            const result = extension.execute[methodKey](this, ...args);
            log('INFO', `Extension '${extensionName}' executed successfully`);
            return result;
        } catch (error) {
            console.log(`Error executing extension '${extensionName}':`, error.message);
            log('ERROR', `Error executing extension '${extensionName}': ${error.message}`);
        }
    }


    showDatabases() {
        try {
            const databases = Array.from(this.data.keys());
            if (databases.length === 0) {
                console.log("No databases found");
            } else {
                console.log("Databases:");
                databases.forEach(db => console.log(db));
            }
            //log('INFO', 'Databases listed successfully');
        } catch (error) {
            handleError(error, 'listing databases');
        }
    }

    showTables() {
        try {
            if (!this.currentDatabase) {
                console.log("No database selected");
                return;
            }
            const tables = this.data.has(this.currentDatabase) ? Array.from(this.data.get(this.currentDatabase).keys()) : [];
            if (tables.length === 0) {
                console.log("No tables found in the current database");
            } else {
                console.log("Tables in current database:");
                tables.forEach(table => console.log(table));
            }
            log('INFO', 'Tables listed successfully');
        } catch (error) {
            handleError(error, 'listing tables');
        }
    }


    initializeCentroids(dataPoints, k) {
        const indices = Array.from({ length: dataPoints.length }, (_, i) => i);
        const randomIndices = indices.sort(() => Math.random() - 0.5).slice(0, k);
        return randomIndices.map(index => dataPoints[index]);
    }

    assignClusters(dataPoints, centroids) {
        const clusters = Array.from({ length: centroids.length }, () => []);
        dataPoints.forEach((dataPoint, index) => {
            const distances = centroids.map(centroid => this.euclideanDistance(dataPoint, centroid));
            const closestCentroidIndex = distances.indexOf(Math.min(...distances));
            clusters[closestCentroidIndex].push(index);
        });
        return clusters;
    }

    calculateCentroids(clusters, dataPoints) {
        return clusters.map(cluster => {
            if (cluster.length === 0) {
                return dataPoints[Math.floor(Math.random() * dataPoints.length)];
            }
            const sum = cluster.reduce((acc, index) => {
                return acc.map((sum, i) => sum + dataPoints[index][i]);
            }, Array(dataPoints[0].length).fill(0));
            return sum.map(sum => sum / cluster.length);
        });
    }

    euclideanDistance(point1, point2) {
        return Math.sqrt(point1.reduce((sum, value, index) => sum + Math.pow(value - point2[index], 2), 0));
    }

    haveConverged(oldCentroids, newCentroids) {
        return oldCentroids.every((oldCentroid, index) => {
            return this.euclideanDistance(oldCentroid, newCentroids[index]) < 1e-5;
        });
    }
    // 批量插入数据
    async insertBatch(tableName, dataArray) {
        try {
            if (!this.currentDatabase) {
                console.log("No database selected");
                return;
            }
            if (!this.data.has(this.currentDatabase) || !this.data.get(this.currentDatabase).has(tableName)) {
                console.log("Table does not exist");
                return;
            }

            if (this.acquireLock(tableName)) {
                const tableData = this.data.get(this.currentDatabase).get(tableName);

                // 确保 tableData 是一个 PartitionedTable 实例
                if (!(tableData instanceof PartitionedTable)) {
                    console.log("Invalid table data structure");
                    this.releaseLock(tableName);
                    return;
                }

                // 批量插入数据
                dataArray.forEach(data => {
                    const validData = {};
                    Object.keys(data).forEach(key => {
                        if (typeof key === 'string' && key.trim() !== '') {
                            validData[key.trim()] = data[key];
                        }
                    });

                    tableData.insert(validData);

                    // 更新索引
                    Object.keys(validData).forEach(dim => {
                        if (this.indexes.has(tableName) && this.indexes.get(tableName).has(dim)) {
                            const indexValue = validData[dim];
                            if (!this.indexes.get(tableName).get(dim).has(indexValue)) {
                                this.indexes.get(tableName).get(dim).set(indexValue, []);
                            }
                            this.indexes.get(tableName).get(dim).get(indexValue).push(validData);
                        }
                    });
                    // 动态更新复合索引s
                    if (this.indexes.has(tableName)) {
                        for (const [indexKey, indexObj] of this.indexes.get(tableName).entries()) {
                            if (indexObj instanceof DimensionsTree) {
                                const dims = indexObj.dimensions;
                                const keys = dims.map(dim => data[dim]);
                                indexObj.insert(keys, data);
                            }
                        }
                    }
                    // 动态更新 PyramidIndex
                    if (this.pyramidIndex && Array.isArray(this.pyramidIndex.data)) {
                        this.pyramidIndex.data.push(data);
                        if (typeof this.pyramidIndex.insert === 'function') {
                            this.pyramidIndex.insert(data);
                        }
                    }
                    // 动态更新 RecursiveSphereWeaving
                    if (this.recursiveSphereWeaving && typeof this.recursiveSphereWeaving.insert === 'function') {
                        this.recursiveSphereWeaving.insert(data);
                    }
                });

                // 保存黎曼球面映射数据
                //this.riemannSphereDB.saveMappedData();
                this.saveData();
                this.releaseLock(tableName);
                console.log(`Inserted ${dataArray.length} records`);
                //log('INFO', 'Batch insert completed successfully');
            } else {
                console.log("Table is locked");
            }
        } catch (error) {
            handleError(error, 'batch insert');
        }
    }

    // 批量更新数据
    // 批量更新数据
    // HypercubeDB 类中的 updateBatch 方法
    updateBatch(tableName, updates) {
        try {
            if (!Array.isArray(updates) || updates.length === 0) {
                console.log("Invalid or empty updates array");
                return;
            }
            if (!this.currentDatabase) {
                console.log("No database selected");
                return;
            }
            if (!this.data.has(this.currentDatabase) || !this.data.get(this.currentDatabase).has(tableName)) {
                console.log("Table does not exist");
                return;
            }

            if (this.acquireLock(tableName)) {
                const tableData = this.data.get(this.currentDatabase).get(tableName);

                // 确保 tableData 是一个 PartitionedTable 实例
                if (!(tableData instanceof PartitionedTable)) {
                    console.log("Invalid table data structure");
                    this.releaseLock(tableName);
                    return;
                }

                updates.forEach(({ key, newData }) => {
                    const partitionIndex = tableData.getPartitionIndex({ [tableData.partitionKey]: key });
                    const partition = tableData.partitions[partitionIndex];
                    if (partition && partition.has(String(key))) {
                        const oldData = partition.get(String(key));
                        // 一次性更新所有字段
                        Object.assign(oldData, newData);

                        // 更新索引
                        Object.keys(newData).forEach(dim => {
                            if (this.indexes.has(tableName) && this.indexes.get(tableName).has(dim)) {
                                const indexValue = newData[dim];
                                if (!this.indexes.get(tableName).get(dim).has(indexValue)) {
                                    this.indexes.get(tableName).get(dim).set(indexValue, []);
                                }
                                this.indexes.get(tableName).get(dim).get(indexValue).push(oldData);
                            }
                        });

                        // 复合索引
                        if (this.indexes.has(tableName)) {
                            for (const [indexKey, indexObj] of this.indexes.get(tableName).entries()) {
                                if (indexObj instanceof DimensionsTree) {
                                    const dims = indexObj.dimensions;
                                    const keys = dims.map(dim => oldData[dim]);
                                    indexObj.update(keys, oldData, { ...oldData });
                                }
                            }
                        }
                        // 金字塔索引
                        if (this.pyramidIndex && typeof this.pyramidIndex.update === 'function') {
                            this.pyramidIndex.update(oldData, { ...oldData });
                        }
                        // 球面编织索引
                        if (this.recursiveSphereWeaving && typeof this.recursiveSphereWeaving.update === 'function') {
                            this.recursiveSphereWeaving.update(oldData, { ...oldData });
                        }
                    }
                });

                this.saveData();
                this.releaseLock(tableName);
                console.log(`Updated ${updates.length} records`);
            } else {
                console.log("Table is locked");
            }
        } catch (error) {
            handleError(error, 'batch update');
            this.releaseLock(tableName);
        }
    }

    // HypercubeDB 类中的 updateBatchColumn 方法
    updateBatchColumn(tableName, condition, newData) {
        try {
            if (!this.currentDatabase) {
                console.log("No database selected");
                return;
            }
            if (!this.data.has(this.currentDatabase) || !this.data.get(this.currentDatabase).has(tableName)) {
                console.log("Table does not exist");
                return;
            }
            if (!this.acquireLock(tableName)) {
                console.log("Table is locked");
                return;
            }

            const partitionedTable = this.data.get(this.currentDatabase).get(tableName);
            let updatedCount = 0;

            partitionedTable.partitions.forEach(partition => {
                if (partition) {
                    for (let [key, record] of partition.entries()) {
                        if (partitionedTable.isMatch(record, condition)) {
                            Object.assign(record, newData);
                            // 更新索引
                            Object.keys(newData).forEach(dim => {
                                if (this.indexes.has(tableName) && this.indexes.get(tableName).has(dim)) {
                                    const indexValue = newData[dim];
                                    if (!this.indexes.get(tableName).get(dim).has(indexValue)) {
                                        this.indexes.get(tableName).get(dim).set(indexValue, []);
                                    }
                                    this.indexes.get(tableName).get(dim).get(indexValue).push(record);
                                }
                            });
                            // 复合索引
                            if (this.indexes.has(tableName)) {
                                for (const [indexKey, indexObj] of this.indexes.get(tableName).entries()) {
                                    if (indexObj instanceof DimensionsTree) {
                                        const dims = indexObj.dimensions;
                                        const keys = dims.map(dim => record[dim]);
                                        indexObj.update(keys, record, { ...record, ...newData });
                                    }
                                }
                            }
                            // 金字塔索引
                            if (this.pyramidIndex && typeof this.pyramidIndex.update === 'function') {
                                this.pyramidIndex.update(record, { ...record, ...newData });
                            }
                            // 球面编织索引
                            if (this.recursiveSphereWeaving && typeof this.recursiveSphereWeaving.update === 'function') {
                                this.recursiveSphereWeaving.update(record, { ...record, ...newData });
                            }
                            updatedCount++;
                        }
                    }
                }
            });

            this.saveData();
            this.releaseLock(tableName);
            console.log(`Batch column update: ${updatedCount} record(s) updated`);
        } catch (error) {
            this.releaseLock(tableName);
            handleError(error, 'batch column update');
        }
    }

    // 修改后的 loadDataAsync 方法 - 使用SQLite加载
    async loadDataAsync() {
        try {
            const dbFiles = await fs.promises.readdir(process.cwd());
            for (const file of dbFiles) {
                if (!file.endsWith('.db')) continue;
                const dbName = path.basename(file, '.db');
                const sqliteDB = new Database(path.join(process.cwd(), file));
                this.sqliteDBs.set(dbName, sqliteDB);

                // 自动获取所有表
               const tables = await sqliteDB.prepare("SELECT name FROM sqlite_master WHERE type='table'").all();
                const partitionedTables = new Map();

                for (const { name: tableName } of tables) {
                    // 自动分页读取数据
                    const pageSize = 1000;
                    let offset = 0;
                    let allRows = [];
                    while (true) {
                        const rows = await sqliteDB.prepare(`SELECT * FROM ${tableName} LIMIT ? OFFSET ?`).all(pageSize, offset);
                        if (rows.length === 0) break;
                        allRows.push(...rows);
                        offset += pageSize;
                    }
                    // 恢复为 PartitionedTable 实例
                    // 自动推断分区键和分区数（可优化为从表名或约定字段获取）
                    const partitionKey = 'id'; // 或根据你的表结构自动推断
                    const numPartitions = 10;  // 或根据你的表结构自动推断
                    const partitionedTable = new PartitionedTable(tableName, partitionKey, numPartitions);
                    for (const row of allRows) {
                        partitionedTable.insert(row);
                    }
                    partitionedTables.set(tableName, partitionedTable);
                }
                this.data.set(dbName, partitionedTables);
            }
            log('INFO', 'All databases loaded from SQLite');
        } catch (error) {
            log('ERROR', `Failed to load databases: ${error.message}`);
        }
    }


    // 异步保存数据库文件（不再整体gzip压缩，只做JSON序列化，分区压缩由分区自身处理）
    // ...existing code...
    // 异步保存数据库文件（只保存当前数据库）
    // 修改后的 saveData 方法 - 使用队列异步保存
    // 保存所有数据（自动分页，数据为中心）
// 保存所有数据库到 SQLite（自动建表，避免主键重复）
async saveData() {
    try {
        for (const [dbName, tables] of this.data.entries()) {
            let sqliteDB = this.sqliteDBs.get(dbName);
            if (!sqliteDB) {
                sqliteDB = new Database(path.join(process.cwd(), `${dbName}.db`));
                this.sqliteDBs.set(dbName, sqliteDB);
            }
          await  sqliteDB.exec('BEGIN TRANSACTION');
            try {
                for (const [tableName, tableObj] of tables.entries()) {
                    if (!(tableObj instanceof PartitionedTable)) continue;
                    // 取第一个分区第一个数据作为样本
                    let sample = null;
                    for (const partition of tableObj.partitions) {
                        if (partition && partition.size > 0) {
                            sample = partition.values().next().value;
                            break;
                        }
                    }
                    if (!sample) continue;
                    // 主键字段
                    const pk = tableObj.partitionKey || 'id';
                    // 过滤掉主键字段，避免重复
                    const columnsArr = Object.keys(sample).filter(col => col !== pk);
                    const columns = columnsArr.map(col => `${col} TEXT`).join(',');
                 await   sqliteDB.exec(`CREATE TABLE IF NOT EXISTS ${tableName} (${columns}, ${pk} TEXT PRIMARY KEY)`);

                    // 清空旧数据
                await    sqliteDB.exec(`DELETE FROM ${tableName}`);

                    // 分区遍历插入
                    const insertStmt =   await sqliteDB.prepare(
                        `INSERT INTO ${tableName} (${[...columnsArr, pk].join(',')}) VALUES (${[...columnsArr.map(() => '?'), '?'].join(',')})`
                    );
                    let batch = [];
                    for (const partition of tableObj.partitions) {
                        if (!partition) continue;
                        for (const [id, row] of partition.entries()) {
                            // 按顺序取出非主键字段和主键
                            batch.push([...columnsArr.map(col => row[col]), id]);
                            if (batch.length >= 1000) {
                                for (const params of batch) insertStmt.run(...params);
                                batch = [];
                            }
                        }
                    }
                    for (const params of batch) insertStmt.run(...params);
                }
               await   sqliteDB.exec('COMMIT');
            } catch (txError) {
               await   sqliteDB.exec('ROLLBACK');
                log('ERROR', `Transaction failed for ${dbName}: ${txError.message}`);
            }
        }
        log('INFO', 'All databases saved to SQLite');
    } catch (error) {
        log('ERROR', `Save operation failed: ${error.message}`);
    }
}

    // 新增方法：处理实际的保存操作
    async _processSave(dbName) {
        if (!this.data.has(dbName)) {
            log('INFO', `No data to save for database: ${dbName}`);
            this.pendingSaves.delete(dbName);
            return;
        }

        try {
            // 如果没有SQLite连接，创建一个
            if (!this.sqliteDBs.has(dbName)) {
                const dbPath = path.resolve(process.cwd(), `${dbName}.db`);
                this.sqliteDBs.set(dbName, new Database(dbPath, { verbose: log }));

                // 创建元数据表
                this.sqliteDBs.get(dbName).exec(`
                CREATE TABLE IF NOT EXISTS _dbms_meta (
                    table_name TEXT,
                    key TEXT, 
                    value TEXT,
                    PRIMARY KEY (table_name, key)
                )
            `);
            }

            const sqliteDB = this.sqliteDBs.get(dbName);
            const dbData = this.data.get(dbName);

            // 开启事务
           await   sqliteDB.exec('BEGIN TRANSACTION');

            try {
                for (const [tableName, table] of dbData.entries()) {
                    if (!(table instanceof PartitionedTable)) continue;

                    // 确保表存在
                    const tableExists =  await sqliteDB.prepare(`SELECT name FROM sqlite_master WHERE type='table' AND name='${tableName}_data'`).get();
                    if (!tableExists) {
                      await    sqliteDB.exec(`
                        CREATE TABLE ${tableName}_data (
                            id TEXT PRIMARY KEY,
                            partition_index INTEGER,
                            data TEXT
                        );
                        CREATE INDEX idx_${tableName}_partition ON ${tableName}_data(partition_index);
                    `);

                        // 保存表配置到元数据
                        const configStmt =  await sqliteDB.prepare('INSERT OR REPLACE INTO _dbms_meta VALUES (?, ?, ?)');
                        configStmt.run(tableName, 'config', JSON.stringify({
                            partitionKey: table.partitionKey,
                            numPartitions: table.numPartitions,
                            compressed: table.compressed
                        }));
                    }

                    // 准备插入/更新语句
                    const insertStmt =  await sqliteDB.prepare(`INSERT OR REPLACE INTO ${tableName}_data VALUES (?, ?, ?)`);

                    // 遍历所有分区数据
                    for (let partitionIndex = 0; partitionIndex < table.partitions.length; partitionIndex++) {
                        const partition = table.partitions[partitionIndex];
                        if (!partition) continue;

                        for (const [key, value] of partition.entries()) {
                            let dataToStore;
                            if (table.compressed && value) {
                                // 压缩数据
                                const compressed = Array.isArray(value)
                                    ? { compressed: this.conicalProjectionCompress(value) }
                                    : value;
                                dataToStore = JSON.stringify(compressed);
                            } else {
                                // 不压缩
                                dataToStore = JSON.stringify(value);
                            }

                            // 插入/更新数据
                            insertStmt.run(key, partitionIndex, dataToStore);
                        }
                    }
                }

                // 提交事务
              await    sqliteDB.exec('COMMIT');
                log('INFO', `Database ${dbName} saved to SQLite backend successfully`);
            } catch (txError) {
                // 回滚事务
                await  sqliteDB.exec('ROLLBACK');
                log('ERROR', `Transaction failed for ${dbName}: ${txError.message}`);
            }
        } catch (error) {
            log('ERROR', `Save operation failed for ${dbName}: ${error.message}`);
        } finally {
            this.pendingSaves.delete(dbName);
        }
    }
    // 新增方法：启动保存队列处理器
    _startSaveQueueProcessor() {
        // 每秒检查一次是否有待保存的数据库
        setInterval(async () => {
            if (this.pendingSaves.size > 0 && this.saveQueue.length < 5) {
                // 确保队列中的任务不过多
                for (const dbName of this.pendingSaves) {
                    this.saveQueue.push(async () => {
                        await this._processSave(dbName);
                    });
                }
            }
        }, 1000);
    }

    // 修改关闭方法，确保所有数据保存完毕
    async close() {
        // 等待所有保存队列完成
        await new Promise(resolve => {
            if (this.saveQueue.length === 0) {
                resolve();
                return;
            }

            this.saveQueue.once('end', resolve);
        });

        // 关闭所有SQLite连接
        for (const [dbName, sqliteDB] of this.sqliteDBs.entries()) {
           await   sqliteDB.close();
        }

        log('INFO', 'Database system shutdown successfully');
    }
    // ...existing code...
    acquireLock(tableName) {
        if (!this.locks.has(tableName)) {
            this.locks.set(tableName, true);
            //log('DEBUG', `Lock acquired for table ${tableName}`);
            return true;
        }
        //log('DEBUG', `Failed to acquire lock for table ${tableName}`);
        return false;
    }

    releaseLock(tableName) {
        this.locks.delete(tableName);
        //log('DEBUG', `Lock released for table ${tableName}`);
    }
    // 创建复合索引方法
    createCompositeIndex(tableName, dimensions) {
        try {
            if (!this.currentDatabase) {
                console.log("No database selected");
                return;
            }
            if (!this.data.has(this.currentDatabase) || !this.data.get(this.currentDatabase).has(tableName)) {
                console.log("Table does not exist");
                return;
            }

            if (this.acquireLock(tableName)) {
                // 索引key为维度顺序拼接
                const indexKey = dimensions.join('|');
                if (!this.indexes.has(tableName)) {
                    this.indexes.set(tableName, new Map());
                }

                if (!this.indexes.get(tableName).has(indexKey)) {
                    const dimensionTree = new DimensionsTree(dimensions);
                    // ...插入数据...
                    this.indexes.get(tableName).set(indexKey, dimensionTree);
                    // 保存数据
                    //this.riemannSphereDB.saveMappedData();
                    //this.saveData();
                    console.log(`Composite index on ${dimensions.join(', ')} added`);
                    this.dimensiontree = dimensionTree; // 只在新建时赋值
                } else {
                    console.log("Composite index already exists for these dimensions");
                }

                this.releaseLock(tableName);
                log('INFO', 'Composite index added successfully');
            } else {
                console.log("Table is locked");
            }
        } catch (error) {
            handleError(error, 'adding composite index');
        }
    }

    // 查询复合索引方法（自动适配维度顺序）
    // 查询复合索引方法（更健壮的无序匹配）
    queryCompositeIndexRange(tableName, dims) {
        try {
            if (!this.currentDatabase) {
                console.log("No database selected");
                return;
            }
            if (!this.data.has(this.currentDatabase) || !this.data.get(this.currentDatabase).has(tableName)) {
                console.log("Table does not exist");
                return;
            }
            if (!this.indexes.has(tableName)) {
                console.log("No composite index for this table");
                return;
            }
            // 输出所有索引key和本次查询的维度
            const indexMap = this.indexes.get(tableName);
            const queryDims = Object.keys(dims);
            console.log('已存在的索引key:', Array.from(indexMap.keys()));
            console.log('本次查询的维度:', queryDims);

            // 只要字段集合一致就匹配
            let foundKey = null;
            for (const key of indexMap.keys()) {
                const keyDims = key.split('|');
                if (keyDims.length === queryDims.length &&
                    keyDims.every(d => queryDims.includes(d)) &&
                    queryDims.every(d => keyDims.includes(d))) {
                    foundKey = key;
                    break;
                }
            }
            if (!foundKey) {
                console.log("Composite index does not exist for these dimensions");
                return;
            }
            const dimensionTree = indexMap.get(foundKey);
            // 按索引key的顺序构造range
            const keyDims = foundKey.split('|');
            const range = keyDims.map(dim => {
                const val = dims[dim];
                if (Array.isArray(val) && val.length === 2) {
                    return val;
                } else {
                    return [val, val];
                }
            });
            const results = dimensionTree.query(range);
            console.log(results);
            return results;
        } catch (error) {
            console.log(error.message);
        }
    }

    createDatabase(name) {
        try {
            if (this.data.has(name)) {
                console.log("Database already exists");
            } else {
                this.data.set(name, new Map());
                // 保存黎曼球面映射数据
                //this.riemannSphereDB.saveMappedData();
                this.saveData();
                console.log("Database created");
            }
            log('INFO', 'Database created successfully');
        } catch (error) {
            handleError(error, 'creating database');
        }
    }

    createTable(tableName, schema, partitionKey = 'id', numPartitions = 10) {
        try {
            if (!this.currentDatabase) {
                console.log("No database selected");
                return;
            }
            if (this.data.has(this.currentDatabase) && this.data.get(this.currentDatabase).has(tableName)) {
                console.log("Table already exists");
            } else {
                if (this.acquireLock(tableName)) {
                    if (!this.data.has(this.currentDatabase)) {
                        this.data.set(this.currentDatabase, new Map());
                    }
                    // 支持指定分区数
                    const partitionedTable = new PartitionedTable(tableName, partitionKey, numPartitions);
                    this.data.get(this.currentDatabase).set(tableName, partitionedTable);
                    this.releaseLock(tableName);
                    console.log(`Table created with partitioning by ${partitionKey}, partitions: ${numPartitions}`);
                } else {
                    console.log("Table is locked");
                }
            }
            log('INFO', 'Table created successfully');
        } catch (error) {
            handleError(error, 'creating table');
        }
    }


    useDatabase(name) {
        try {
            if (this.data.has(name)) {
                this.currentDatabase = name;
                console.log("Database selected");
            } else {
                console.log("Database does not exist");
            }
            log('INFO', 'Database selected successfully');
        } catch (error) {
            handleError(error, 'using database');
        }
    }


    // 修改后的 createTable 方法
    createTable(tableName, schema, partitionKey = 'id') {
        try {
            if (!this.currentDatabase) {
                console.log("No database selected");
                return;
            }
            if (this.data.has(this.currentDatabase) && this.data.get(this.currentDatabase).has(tableName)) {
                console.log("Table already exists");
            } else {
                if (this.acquireLock(tableName)) {
                    if (!this.data.has(this.currentDatabase)) {
                        this.data.set(this.currentDatabase, new Map());
                    }
                    const partitionedTable = new PartitionedTable(tableName, partitionKey);
                    this.data.get(this.currentDatabase).set(tableName, partitionedTable);
                    this.releaseLock(tableName);
                    console.log(`Table created with partitioning by ${partitionKey}`);
                } else {
                    console.log("Table is locked");
                }
            }
            log('INFO', 'Table created successfully');
        } catch (error) {
            handleError(error, 'creating table');
        }
    }


    addDimension(tableName, dimension) {
        try {
            if (!this.currentDatabase) {
                console.log("No database selected");
                return;
            }
            if (!this.data.has(this.currentDatabase) || !this.data.get(this.currentDatabase).has(tableName)) {
                console.log("Table does not exist");
                return;
            }
            if (this.acquireLock(tableName)) {
                // 初始化索引结构
                if (!this.indexes.has(tableName)) {
                    this.indexes.set(tableName, new Map());
                }
                if (!this.indexes.get(tableName).has(dimension)) {
                    this.indexes.get(tableName).set(dimension, new Map());
                    // 遍历所有分区和分区内数据
                    const partitionedTable = this.data.get(this.currentDatabase).get(tableName);
                    partitionedTable.partitions.forEach(partition => {
                        if (partition) {
                            for (const data of partition.values()) {
                                // 如果没有该字段，可以补充为 null 或默认值
                                if (data[dimension] === undefined) {
                                    data[dimension] = null;
                                }
                                const value = data[dimension];
                                if (!this.indexes.get(tableName).get(dimension).has(value)) {
                                    this.indexes.get(tableName).get(dimension).set(value, []);
                                }
                                this.indexes.get(tableName).get(dimension).get(value).push(data);
                            }
                        }
                    });
                }
                this.saveData();
                this.releaseLock(tableName);
                console.log(`Dimension ${dimension} added`);
            } else {
                console.log("Table is locked");
            }
        } catch (error) {
            handleError(error, 'adding dimension');
        }
    }

    removeDimension(tableName, dimension) {
        try {
            if (!this.currentDatabase) {
                console.log("No database selected");
                return;
            }
            if (!this.data.has(this.currentDatabase) || !this.data.get(this.currentDatabase).has(tableName)) {
                console.log("Table does not exist");
                return;
            }
            if (this.acquireLock(tableName)) {
                if (this.indexes.has(tableName) && this.indexes.get(tableName).has(dimension)) {
                    this.indexes.get(tableName).delete(dimension);
                }
                // 遍历所有分区和分区内数据
                const partitionedTable = this.data.get(this.currentDatabase).get(tableName);
                partitionedTable.partitions.forEach(partition => {
                    if (partition) {
                        for (const data of partition.values()) {
                            delete data[dimension];
                        }
                    }
                });
                this.saveData();
                this.releaseLock(tableName);
                console.log(`Dimension ${dimension} removed`);
            } else {
                console.log("Table is locked");
            }
        } catch (error) {
            handleError(error, 'Removing dimension');
            this.releaseLock(tableName);
        }
    }

    // main.js
    insertData(tableName, data) {
        try {
            if (!this.currentDatabase) {
                console.log("No database selected");
                return;
            }
            if (!this.data.has(this.currentDatabase) || !this.data.get(this.currentDatabase).has(tableName)) {
                console.log(`Table ${tableName} does not exist in the current database`);
                return;
            }
            if (this.acquireLock(tableName)) {
                const partitionedTable = this.data.get(this.currentDatabase).get(tableName);
                if (!(partitionedTable instanceof PartitionedTable)) {
                    console.log(`Invalid table data structure for ${tableName}`);
                    this.releaseLock(tableName);
                    return;
                }

                console.log('Inserting data:', data);

                if (typeof data !== 'object' || data === null) {
                    console.log("Invalid data format");
                    this.releaseLock(tableName);
                    return;
                }

                if (!data[partitionedTable.partitionKey]) {
                    console.log(`Missing required field: ${partitionedTable.partitionKey}`);
                    this.releaseLock(tableName);
                    return;
                }

                const validData = {};
                Object.keys(data).forEach(key => {
                    if (typeof key === 'string' && key.trim() !== '' && data[key] !== 0) {
                        validData[key.trim()] = data[key];
                    } else if (typeof key === 'string' && key.trim() !== '') {
                        console.log(`Ignoring field ${key} with value 0`);
                    }
                });

                data = validData;

                partitionedTable.insert(data);
                // 动态更新复合索引（DimensionsTree）
                if (this.indexes.has(tableName)) {
                    for (const [indexKey, indexObj] of this.indexes.get(tableName).entries()) {
                        if (indexObj instanceof DimensionsTree) {
                            const dims = indexObj.dimensions;
                            const keys = dims.map(dim => data[dim]);
                            indexObj.insert(keys, data);
                        }
                    }
                }
                // 动态更新 PyramidIndex
                if (this.pyramidIndex && Array.isArray(this.pyramidIndex.data)) {
                    this.pyramidIndex.data.push(data);
                    if (typeof this.pyramidIndex.insert === 'function') {
                        this.pyramidIndex.insert(data);
                    }
                }
                // 动态更新 RecursiveSphereWeaving
                if (this.recursiveSphereWeaving && typeof this.recursiveSphereWeaving.insert === 'function') {
                    this.recursiveSphereWeaving.insert(data);
                }
                // 更新索引
                Object.keys(data).forEach(dim => {
                    if (!this.indexes.has(tableName)) {
                        this.indexes.set(tableName, new Map());
                    }
                    if (!this.indexes.get(tableName).has(dim)) {
                        this.indexes.get(tableName).set(dim, new Map());
                    }
                    const indexValue = data[dim];
                    if (!this.indexes.get(tableName).get(dim).has(indexValue)) {
                        this.indexes.get(tableName).get(dim).set(indexValue, []);
                    }
                    this.indexes.get(tableName).get(dim).get(indexValue).push(data);
                });

                // 保存黎曼球面映射数据
                //this.riemannSphereDB.saveMappedData();
                this.saveData();
                this.releaseLock(tableName);
                console.log("Data inserted");
                log('INFO', 'Data inserted successfully');
            } else {
                console.log("Table is locked");
            }
        } catch (error) {
            handleError(error, 'inserting data');
        }
    }


    insertIntoDistanceHash(tableName, data) {
        if (!this.distanceHashes.has(tableName)) {
            this.distanceHashes.set(tableName, new Map());
        }

        const dimensions = Object.keys(data);
        const numDimensions = dimensions.length;

        // 假设我们使用一个简单的哈希函数，将数据点映射到多个哈希桶中
        const numBuckets = 10; // 哈希桶的数量
        const bucketSize = 0.1; // 每个哈希桶的大小

        for (let i = 0; i < numBuckets; i++) {
            const bucketKey = i * bucketSize;
            if (!this.distanceHashes.get(tableName).has(bucketKey)) {
                this.distanceHashes.get(tableName).set(bucketKey, []);
            }

            // 计算数据点到原点的距离
            const distance = Math.sqrt(dimensions.reduce((sum, dim) => sum + Math.pow(data[dim], 2), 0));

            // 将数据点插入到相应的哈希桶中
            if (distance >= bucketKey && distance < bucketKey + bucketSize) {
                this.distanceHashes.get(tableName).get(bucketKey).push(data);
            }
        }
    }

    dropTable(tableName) {
        try {
            if (!this.currentDatabase) {
                console.log("No database selected");
                return;
            }
            if (this.acquireLock(tableName)) {
                if (this.data.has(this.currentDatabase) && this.data.get(this.currentDatabase).has(tableName)) {
                    // 删除表的数据
                    this.data.get(this.currentDatabase).delete(tableName);
                    // 删除表的索引
                    this.indexes.delete(tableName);
                    // 删除表的锁
                    this.locks.delete(tableName);
                    // 删除表的距离哈希
                    this.distanceHashes.delete(tableName);
                    // 删除表的查询缓存
                    this.queryCache.forEach((value, key) => {
                        if (key.tableName === tableName) {
                            this.queryCache.delete(key);
                        }
                    });
                    // 保存黎曼球面映射数据
                    //this.riemannSphereDB.saveMappedData();
                    this.saveData();
                    this.releaseLock(tableName);
                    console.log("Table dropped");
                } else {
                    console.log("Table does not exist");
                }
                log('INFO', 'Table dropped successfully');
            } else {
                console.log("Table is locked");
            }
        } catch (error) {
            handleError(error, 'dropping table');
        }
    }
    /*注意断点，此处问题为partitionTable.partitionKey的record的几次反义以后（由b->4->b->4）后（执行指令为Selected data: [ { b: 4, c: 4 } ]
    2025-04-06T14:31:42.276Z [DEBUG] Lock released for table a
    > delete from a where b>0
    2025-04-06T14:33:11.766Z [DEBUG] Lock acquired for table a
    >2025-04-06T14:33:59.153Z [ERROR] Invalid sphere mapping result
    2025-04-06T14:33:59.159Z [ERROR] Compression failed for 3: crypto.createHash is not a function
    > 2025-04-06T14:33:59.216Z [INFO] Database test saved successfully
    2025-04-06T14:33:59.218Z [DEBUG] Lock released for table a
    Deleted 0 records
    2025-04-06T14:34:27.290Z [INFO] Deleted 0 records from a
    > delete from a where b>0
    2025-04-06T14:35:53.302Z [DEBUG] Lock acquired for table a
    ）时在partition中找不到hashmap的key，导致删除失败。
    */
    // 修复后的删除方法（带压缩状态检查）

    // 优化后的 deleteData 方法，支持主键O(1)和复杂条件O(n)
    async deleteData(tableName, condition) {
        try {
            if (!this.currentDatabase) return 0;
            const db = this.data.get(this.currentDatabase);
            if (!db || !db.has(tableName)) return 0;
            if (!this.acquireLock(tableName)) return 0;

            const partitionedTable = db.get(tableName);
            if (!(partitionedTable instanceof PartitionedTable)) {
                this.releaseLock(tableName);
                return 0;
            }

            const pk = partitionedTable.partitionKey;
            let deletedCount = 0;

            // 主键精确匹配 O(1)
            if (
                condition &&
                condition[pk] &&
                typeof condition[pk] === 'object' &&
                condition[pk]._op === '='
            ) {
                const key = String(condition[pk]._value);
                const partitionIndex = partitionedTable.getPartitionIndex({ [pk]: key });
                const partition = partitionedTable.partitions[partitionIndex];
                if (partition && partition.has(key)) {
                    const record = partition.get(key);

                    // 索引同步删除
                    Object.keys(record).forEach(dim => {
                        if (this.indexes.get(tableName)?.has(dim)) {
                            const dimMap = this.indexes.get(tableName).get(dim);
                            const value = record[dim];
                            if (dimMap.has(value)) {
                                dimMap.set(value, dimMap.get(value).filter(item => item[pk] !== key));
                            }
                        }
                    });
                    // 复合索引
                    if (this.indexes.has(tableName)) {
                        for (const [indexKey, indexObj] of this.indexes.get(tableName).entries()) {
                            if (indexObj instanceof DimensionsTree) {
                                const dims = indexObj.dimensions;
                                const keys = dims.map(dim => record[dim]);
                                indexObj.delete(keys, record);
                            }
                        }
                    }
                    // 金字塔索引
                    if (this.pyramidIndex && typeof this.pyramidIndex.delete === 'function') {
                        this.pyramidIndex.delete(record);
                    }
                    // 球面编织索引
                    if (this.recursiveSphereWeaving && typeof this.recursiveSphereWeaving.delete === 'function') {
                        this.recursiveSphereWeaving.delete(record);
                    }

                    partition.delete(key);
                    deletedCount = 1;
                }
            } else {
                // 非主键条件 O(n) 遍历
                partitionedTable.partitions.forEach(partition => {
                    if (partition) {
                        for (let [key, record] of Array.from(partition.entries())) {
                            if (partitionedTable.isMatch(record, condition)) {
                                // 索引同步删除
                                Object.keys(record).forEach(dim => {
                                    if (this.indexes.get(tableName)?.has(dim)) {
                                        const dimMap = this.indexes.get(tableName).get(dim);
                                        const value = record[dim];
                                        if (dimMap.has(value)) {
                                            dimMap.set(value, dimMap.get(value).filter(item => item[pk] !== key));
                                        }
                                    }
                                });
                                // 复合索引
                                if (this.indexes.has(tableName)) {
                                    for (const [indexKey, indexObj] of this.indexes.get(tableName).entries()) {
                                        if (indexObj instanceof DimensionsTree) {
                                            const dims = indexObj.dimensions;
                                            const keys = dims.map(dim => record[dim]);
                                            indexObj.delete(keys, record);
                                        }
                                    }
                                }
                                // 金字塔索引
                                if (this.pyramidIndex && typeof this.pyramidIndex.delete === 'function') {
                                    this.pyramidIndex.delete(record);
                                }
                                // 球面编织索引
                                if (this.recursiveSphereWeaving && typeof this.recursiveSphereWeaving.delete === 'function') {
                                    this.recursiveSphereWeaving.delete(record);
                                }

                                partition.delete(key);
                                deletedCount++;
                            }
                        }
                    }
                });
            }

            this.saveData();
            this.releaseLock(tableName);
            console.log(`Deleted ${deletedCount} record(s)`);
            return deletedCount;
        } catch (error) {
            this.releaseLock(tableName);
            throw error;
        }
    }

    // 优化后的 updateData 方法，支持主键O(1)和复杂条件O(n)
    updateData(tableName, condition, newData) {
        try {
            if (!this.currentDatabase) return;
            if (!this.data.has(this.currentDatabase) || !this.data.get(this.currentDatabase).has(tableName)) return;
            if (!this.acquireLock(tableName)) return;

            const partitionedTable = this.data.get(this.currentDatabase).get(tableName);
            const pk = partitionedTable.partitionKey;
            let updatedCount = 0;

            // 主键精确匹配 O(1)
            if (
                condition &&
                condition[pk] &&
                typeof condition[pk] === 'object' &&
                condition[pk]._op === '='
            ) {
                const key = String(condition[pk]._value);
                const partitionIndex = partitionedTable.getPartitionIndex({ [pk]: key });
                const partition = partitionedTable.partitions[partitionIndex];
                if (partition && partition.has(key)) {
                    const record = partition.get(key);

                    // 索引同步更新
                    Object.assign(record, newData);
                    Object.keys(newData).forEach(dim => {
                        if (this.indexes.has(tableName) && this.indexes.get(tableName).has(dim)) {
                            const indexValue = newData[dim];
                            if (!this.indexes.get(tableName).get(dim).has(indexValue)) {
                                this.indexes.get(tableName).get(dim).set(indexValue, []);
                            }
                            this.indexes.get(tableName).get(dim).get(indexValue).push(record);
                        }
                    });
                    // 复合索引
                    if (this.indexes.has(tableName)) {
                        for (const [indexKey, indexObj] of this.indexes.get(tableName).entries()) {
                            if (indexObj instanceof DimensionsTree) {
                                const dims = indexObj.dimensions;
                                const keys = dims.map(dim => record[dim]);
                                indexObj.update(keys, record, { ...record, ...newData });
                            }
                        }
                    }
                    // 金字塔索引
                    if (this.pyramidIndex && typeof this.pyramidIndex.update === 'function') {
                        this.pyramidIndex.update(record, { ...record, ...newData });
                    }
                    // 球面编织索引
                    if (this.recursiveSphereWeaving && typeof this.recursiveSphereWeaving.update === 'function') {
                        this.recursiveSphereWeaving.update(record, { ...record, ...newData });
                    }
                    updatedCount = 1;
                }
            } else {
                // 非主键条件 O(n) 遍历
                partitionedTable.partitions.forEach(partition => {
                    if (partition) {
                        for (let [key, record] of partition.entries()) {
                            if (partitionedTable.isMatch(record, condition)) {
                                Object.assign(record, newData);
                                Object.keys(newData).forEach(dim => {
                                    if (this.indexes.has(tableName) && this.indexes.get(tableName).has(dim)) {
                                        const indexValue = newData[dim];
                                        if (!this.indexes.get(tableName).get(dim).has(indexValue)) {
                                            this.indexes.get(tableName).get(dim).set(indexValue, []);
                                        }
                                        this.indexes.get(tableName).get(dim).get(indexValue).push(record);
                                    }
                                });
                                // 复合索引
                                if (this.indexes.has(tableName)) {
                                    for (const [indexKey, indexObj] of this.indexes.get(tableName).entries()) {
                                        if (indexObj instanceof DimensionsTree) {
                                            const dims = indexObj.dimensions;
                                            const keys = dims.map(dim => record[dim]);
                                            indexObj.update(keys, record, { ...record, ...newData });
                                        }
                                    }
                                }
                                // 金字塔索引
                                if (this.pyramidIndex && typeof this.pyramidIndex.update === 'function') {
                                    this.pyramidIndex.update(record, { ...record, ...newData });
                                }
                                // 球面编织索引
                                if (this.recursiveSphereWeaving && typeof this.recursiveSphereWeaving.update === 'function') {
                                    this.recursiveSphereWeaving.update(record, { ...record, ...newData });
                                }
                                updatedCount++;
                            }
                        }
                    }
                });
            }

            this.saveData();
            this.releaseLock(tableName);
            console.log(`Updated ${updatedCount} record(s)`);
        } catch (error) {
            this.releaseLock(tableName);
            throw error;
        }
    }

    // HypercubeDB 类中的 selectData 方法
    selectData(tableName, query, fields = ['*']) {
        try {
            if (!this.currentDatabase) {
                console.log("No database selected");
                return;
            }
            const currentDB = this.data.get(this.currentDatabase);
            if (!currentDB) {
                console.log("Database does not exist");
                return;
            }
            if (!currentDB.has(tableName)) {
                console.log("Table does not exist");
                return;
            }
            if (this.acquireLock(tableName)) {
                try {
                    const cacheKey = JSON.stringify({ tableName, query });
                    this.queryCache.delete(cacheKey); // 强制清除旧缓存

                    const partitionedTable = currentDB.get(tableName);
                    //console.log('partitionedTable:', partitionedTable); // 添加调试信息

                    if (partitionedTable && partitionedTable instanceof PartitionedTable) {
                        let results = partitionedTable.query(query || {}, fields);

                        // 使用距离哈希加速查询
                        if (query && query.distance !== undefined) {
                            const distance = query.distance;
                            const bucketSize = 0.1;
                            const bucketKey = Math.floor(distance / bucketSize) * bucketSize;
                            const neighborBuckets = [
                                bucketKey - bucketSize,
                                bucketKey,
                                bucketKey + bucketSize
                            ];
                            results = neighborBuckets.reduce((acc, bucket) => {
                                if (this.distanceHashes.get(tableName)?.has(bucket)) {
                                    const bucketResults = this.distanceHashes.get(tableName).get(bucket)
                                        .filter(data => {
                                            const pointDistance = Math.sqrt(
                                                Object.keys(data).reduce((sum, dim) =>
                                                    sum + Math.pow(data[dim], 2), 0)
                                            );
                                            return pointDistance >= distance - bucketSize &&
                                                pointDistance < distance + bucketSize;
                                        })
                                        .map(data => {
                                            if (fields && fields.length && fields[0] !== '*') {
                                                const filtered = {};
                                                fields.forEach(f => {
                                                    if (data.hasOwnProperty(f)) filtered[f] = data[f];
                                                });
                                                return filtered;
                                            }
                                            return { ...data };
                                        });
                                    return [...acc, ...bucketResults];
                                }
                                return acc;
                            }, []);
                            // 去重
                            const seen = new Set();
                            results = results.filter(data => {
                                const key = JSON.stringify(data);
                                return seen.has(key) ? false : seen.add(key);
                            });
                        } else {
                            if (query && query.label !== undefined) {
                                results = results.filter(data => data.label === query.label);
                            }
                            // 去重
                            const seen = new Set();
                            results = results.filter(data => {
                                const key = JSON.stringify(data);
                                return seen.has(key) ? false : seen.add(key);
                            });
                            // 去除 null 值
                            results = results.filter(data => data !== null);
                            // 去除 undefined 值
                            results = results.filter(data => data !== undefined);
                            // 去除 undefined 键值对
                            results = results.map(data => {
                                const filteredData = {};
                                Object.keys(data).forEach(key => {
                                    if (data[key] !== undefined) {
                                        filteredData[key] = data[key];
                                    }
                                });
                                return filteredData;
                            });
                        }

                        // 根据聚类标签查询
                        if (query && query.label !== undefined) {
                            results = results.filter(data => data.label === query.label);
                        }

                        console.log(results);
                        this.queryCache.set(cacheKey, results); // 缓存结果
                        return { data: results }; // 确保返回的数据结构包含 data 属性
                    } else {
                        console.log('partitionedTable is not an instance of PartitionedTable or is undefined');
                    }
                } catch (error) {
                    handleError(error, 'selecting data');
                    return { data: [] };
                } finally {
                    this.releaseLock(tableName);
                }
            } else {
                console.log("Table is locked");
            }
            log('INFO', 'Data selected successfully');
        } catch (error) {
            handleError(error, 'selecting data');
        }
    }
    deleteColumn(tableName, column, query) {
        try {
            if (!this.currentDatabase) {
                console.log("No database selected");
                return;
            }
            if (!this.data.has(this.currentDatabase) || !this.data.get(this.currentDatabase).has(tableName)) {
                console.log("Table does not exist");
                return;
            }
            if (this.acquireLock(tableName)) {
                const partitionedTable = this.data.get(this.currentDatabase).get(tableName);
                let updated = false;

                partitionedTable.partitions.forEach(partition => {
                    if (partition) {
                        for (let [key, data] of partition.entries()) {
                            if (partitionedTable.isMatch(data, query)) {
                                // 删除列
                                if (data.hasOwnProperty(column)) {
                                    delete data[column];
                                    updated = true;
                                }
                            }
                        }
                    }
                });

                if (updated) {
                    // 更新索引
                    this.updateIndexesAfterColumnDeletion(tableName, column);

                    // 保存黎曼球面映射数据
                    //this.riemannSphereDB.saveMappedData();
                    this.saveData();
                    console.log(`Column ${column} deleted from records matching the condition`);
                } else {
                    console.log("No records found matching the condition");
                }

                this.releaseLock(tableName);
                log('INFO', 'Column deleted successfully');
            } else {
                console.log("Table is locked");
            }
        } catch (error) {
            handleError(error, 'deleting column');
            this.releaseLock(tableName); // 确保在发生错误时释放锁
        }
    }

    updateIndexesAfterColumnDeletion(tableName, column) {
        if (this.indexes.has(tableName) && this.indexes.get(tableName).has(column)) {
            this.indexes.get(tableName).delete(column);
        }
    }
    getFuzzyFunction(tableName, dim1, dim2) {
        if (!this.currentDatabase) {
            console.log("No database selected");
            return;
        }
        // 修复：使用正确的数据访问方式
        if (!this.data.has(this.currentDatabase) || !this.data.get(this.currentDatabase).has(tableName)) {
            console.log("Table does not exist");
            return;
        }

        const partitionedTable = this.data.get(this.currentDatabase).get(tableName);
        let dataPoints = [];

        // 从分区表中收集数据
        partitionedTable.partitions.forEach(partition => {
            if (partition) {
                for (const data of partition.values()) {
                    dataPoints.push(data);
                }
            }
        });

        const filteredData = dataPoints.filter(data =>
            data[dim1] !== undefined && data[dim2] !== undefined
        );

        return (x, y) => {
            let sum = 0;
            let count = 0;
            filteredData.forEach(data => {
                // 确保数值转换
                const val1 = Number(data[dim1]) || 0;
                const val2 = Number(data[dim2]) || 0;
                sum += val1 * x + val2 * y;
                count++;
            });
            return count > 0 ? sum / count : 0;
        };
    }

    taylorExpand(tableName, dim1, dim2, x0, y0, order) {
        const fuzzyFunction = this.getFuzzyFunction(tableName, dim1, dim2);
        if (!fuzzyFunction) {
            console.log("Failed to get fuzzy function");
            return [];
        }

        const terms = [];
        for (let i = 0; i <= order; i++) {
            for (let j = 0; j <= order - i; j++) {
                try {
                    const term = this.taylorTerm(fuzzyFunction, x0, y0, i, j);
                    terms.push({
                        power_x: i,
                        power_y: j,
                        coefficient: term
                    });
                } catch (error) {
                    console.log(`Error calculating term (${i},${j}):`, error.message);
                }
            }
        }
        return terms;
    }

    taylorTerm(fuzzyFunction, x0, y0, i, j) {
        const h = 1e-5;
        let derivative = 0;

        // 计算偏导数的数值近似
        try {
            for (let k = 0; k <= i; k++) {
                for (let l = 0; l <= j; l++) {
                    const sign = Math.pow(-1, k + l);
                    const coeff = 1 / (this.factorial(k) * this.factorial(l));
                    const x = x0 + (i - k) * h;
                    const y = y0 + (j - l) * h;
                    derivative += sign * coeff * fuzzyFunction(x, y);
                }
            }
            return derivative * Math.pow(x0, i) * Math.pow(y0, j) / (this.factorial(i) * this.factorial(j));
        } catch (error) {
            return 0;
        }
    }

    factorial(n) {
        if (n === 0 || n === 1) return 1;
        if (n < 0) return 0;
        let result = 1;
        for (let i = 2; i <= n; i++) {
            result *= i;
        }
        return result;
    }

/*    importFromCSV(tableName, filePath) {
        if (!this.currentDatabase) {
            console.log("No database selected");
            return;
        }
        if (!this.data.has(this.currentDatabase) || !this.data.get(this.currentDatabase).has(tableName)) {
            console.log("Table does not exist");
            return;
        }

        if (!this.acquireLock(tableName)) {
            console.log("Table is locked");
            return;
        }

        try {
            let rowCount = 0;
            fs.createReadStream(filePath)
                .pipe(csv())
                .on('data', (row) => {
                    try {
                        // 数据清理和验证
                        const cleanedRow = {};
                        Object.keys(row).forEach(key => {
                            const trimmedKey = key.trim();
                            if (trimmedKey) {
                                let value = row[key];
                                // 尝试转换数值
                                if (!isNaN(value) && value !== '') {
                                    value = Number(value);
                                }
                                cleanedRow[trimmedKey] = value;
                            }
                        });

                        this.insertData(tableName, cleanedRow);
                        rowCount++;
                    } catch (error) {
                        console.log(`Error processing row ${rowCount + 1}:`, error.message);
                    }
                })
                .on('end', () => {
                    console.log(`CSV file successfully imported. ${rowCount} rows processed.`);
                    this.releaseLock(tableName);
                    log('INFO', `CSV import completed: ${rowCount} rows`);
                })
                .on('error', (error) => {
                    console.log('CSV import error:', error.message);
                    this.releaseLock(tableName);
                    handleError(error, 'CSV import');
                });
        } catch (error) {
            this.releaseLock(tableName);
            handleError(error, 'CSV import setup');
        }
    }
*/
    // 高维球面映射算法
    projectToRiemannSphere(tableName, dimensions, viewpoint = [0, 0, 0]) {
        if (!this.currentDatabase || !this.data.has(this.currentDatabase) || !this.data.get(this.currentDatabase).has(tableName)) {
            console.log("Database/Table not selected");
            return [];
        }

        const partitionedTable = this.data.get(this.currentDatabase).get(tableName);
        let points = [];
        partitionedTable.partitions.forEach(partition => {
            if (partition) {
                for (const data of partition.values()) {
                    points.push(data);
                }
            }
        });

        return points.map(data => {
            const rawCoords = dimensions.map(d => parseFloat(data[d]) || 0);
            const magnitude = Math.sqrt(rawCoords.reduce((sum, val) => sum + val * val, 0));
            const projected = rawCoords.map((coord, i) => coord / (1 - (viewpoint[i] || 0)));
            const distanceWeight = 1 / (1 + Math.exp(-magnitude));
            return {
                x: projected[0] * distanceWeight,
                y: projected[1] * distanceWeight,
                z: projected[2] * distanceWeight,
                metadata: data
            };
        });
    }

    // 视角变换响应函数
    calculateViewTransform(phi, theta) {
        // 将球坐标转换为笛卡尔坐标
        return {
            x: Math.sin(theta) * Math.cos(phi),
            y: Math.sin(theta) * Math.sin(phi),
            z: Math.cos(theta)
        };
    }

    // 光线投射查询
    raycastQuery(tableName, origin, direction) {
        const points = this.projectToRiemannSphere(tableName, ['x', 'y', 'z']);
        const results = [];

        points.forEach(point => {
            // 计算点到光线的距离
            const vecToPoint = [point.x - origin[0], point.y - origin[1], point.z - origin[2]];
            const crossProd = [
                direction[1] * vecToPoint[2] - direction[2] * vecToPoint[1],
                direction[2] * vecToPoint[0] - direction[0] * vecToPoint[2],
                direction[0] * vecToPoint[1] - direction[1] * vecToPoint[0]
            ];
            const distance = Math.sqrt(crossProd.reduce((sum, val) => sum + val * val, 0));

            if (distance < 0.1) { // 命中阈值
                results.push({
                    point,
                    screenPos: this._calculateScreenPosition(point, origin, direction),
                    distance
                });
            }
        });

        return results.sort((a, b) => a.distance - b.distance);
    }

    _calculateScreenPosition(point, cameraPos, lookDir) {
        // 简化投影计算（实际需要完整的相机矩阵）
        const offsetX = point.x - cameraPos[0];
        const offsetY = point.y - cameraPos[1];
        const dot = offsetX * lookDir[0] + offsetY * lookDir[1];
        return {
            x: dot * 100, // 简化的屏幕坐标转换
            y: (point.z - cameraPos[2]) * 50
        };
    }
    // 添加到 HypercubeDB 类中
    calculateIntersectionPoints(dimensions, func, sphereRadius = 1, resolution = 0.1) {
        const points = [];
        const min = -sphereRadius;
        const max = sphereRadius;
        const step = resolution;

        // 根据维度数量生成网格点
        if (dimensions.length === 1) {
            for (let x = min; x <= max; x += step) {
                try {
                    const result = func(x);
                    if (Math.abs(result) < step) { // 接近零点
                        const point = {};
                        point[dimensions[0]] = x;
                        points.push(point);
                    }
                } catch (error) {
                    // 跳过错误点
                }
            }
        } else if (dimensions.length === 2) {
            for (let x = min; x <= max; x += step) {
                for (let y = min; y <= max; y += step) {
                    try {
                        const result = func(x, y);
                        if (Math.abs(result) < step) {
                            const point = {};
                            point[dimensions[0]] = x;
                            point[dimensions[1]] = y;
                            points.push(point);
                        }
                    } catch (error) {
                        // 跳过错误点
                    }
                }
            }
        } else if (dimensions.length === 3) {
            for (let x = min; x <= max; x += step) {
                for (let y = min; y <= max; y += step) {
                    for (let z = min; z <= max; z += step) {
                        try {
                            const result = func(x, y, z);
                            if (Math.abs(result) < step) {
                                const point = {};
                                point[dimensions[0]] = x;
                                point[dimensions[1]] = y;
                                point[dimensions[2]] = z;
                                points.push(point);
                            }
                        } catch (error) {
                            // 跳过错误点
                        }
                    }
                }
            }
        }

        return points;
    }

    insertIntersectionPoints(tableName, dimensions, func, sphereRadius = 1, resolution = 0.1) {
        if (!this.currentDatabase) {
            console.log("No database selected");
            return;
        }
        if (!this.data.has(this.currentDatabase) || !this.data.get(this.currentDatabase).has(tableName)) {
            console.log("Table does not exist");
            return;
        }

        if (!this.acquireLock(tableName)) {
            console.log("Table is locked");
            return;
        }

        try {
            const points = this.calculateIntersectionPoints(dimensions, func, sphereRadius, resolution);

            points.forEach(point => {
                this.insertData(tableName, point);
            });

            console.log(`Inserted ${points.length} intersection points`);
            log('INFO', `Intersection points inserted: ${points.length}`);
        } catch (error) {
            handleError(error, 'inserting intersection points');
        } finally {
            this.releaseLock(tableName);
        }
    }


    dropdatabase(databaseName) {
        try {
            if (!this.data.has(databaseName)) {
                console.log(`Database "${databaseName}" does not exist`);
                return;
            }

            this.isDroppingDatabase = true; // 设置标志位
            // 从内存中移除数据库数据
            // 获取数据库级锁
            const dbLock = this.locks.get('_global') || new Map();
            dbLock.set(databaseName, true);

            try {
                this.data.delete(databaseName);
                // 清除所有相关缓存
                this.queryCache.clear();
            } finally {
                dbLock.delete(databaseName);
            }
            this.indexes.delete(databaseName);
            this.locks.delete(databaseName);
            this.distanceHashes.delete(databaseName);
            this.queryCache.forEach((value, key) => {
                if (key.tableName && key.tableName.includes(databaseName)) {
                    this.queryCache.delete(key);
                }
            });

            // 保存数据以确保内存中的更改被写入磁盘
            // 保存黎曼球面映射数据
            //this.riemannSphereDB.saveMappedData();
            this.saveData();

            // 删除数据库文件
            const dbFilePath = path.join(__dirname, `${databaseName}.db`);
            try {
                if (fs.existsSync(dbFilePath)) {
                    fs.unlinkSync(dbFilePath);
                    console.log(`Database "${databaseName}" deleted`);
                }
                // 同时删除临时文件
                const tempPath = path.join(__dirname, `${databaseName}.db.tmp`);
                if (fs.existsSync(tempPath)) { fs.unlinkSync(tempPath); }
            } catch (fileError) {
                console.log(`File cleanup error: ${fileError.message}`);
            }
            this.isDroppingDatabase = false; // 重置标志位
        } catch (error) {
            handleError(error, 'dropping database');
        }
    }
    createPyramidIndex(tableName, maxCapacity = 100, k = 5) {
        if (!this.currentDatabase || !this.data.has(this.currentDatabase) || !this.data.get(this.currentDatabase).has(tableName)) {
            console.log("Table does not exist");
            return;
        }
        // 获取所有分区的数据
        const partitionedTable = this.data.get(this.currentDatabase).get(tableName);
        let data = [];
        partitionedTable.partitions.forEach(partition => {
            if (partition) {
                for (const value of partition.values()) {
                    data.push(value);
                }
            }
        });
        this.pyramidIndex = new PyramidIndex(data, maxCapacity, k);
        console.log("Pyramid index created");
    }

    queryPyramidIndex(queryPoint) {
        if (!this.pyramidIndex) {
            console.log("Pyramid index not created");
            return;
        }
        const results = this.pyramidIndex.query(queryPoint);
        console.log(results);
    }

    createRecursiveSphereWeaving(tableName, dimensions = null) {
        if (!this.currentDatabase || !this.data.has(this.currentDatabase) || !this.data.get(this.currentDatabase).has(tableName)) {
            console.log("Table does not exist");
            return;
        }
        const partitionedTable = this.data.get(this.currentDatabase).get(tableName);
        let data = [];
        partitionedTable.partitions.forEach(partition => {
            if (partition) {
                for (const value of partition.values()) {
                    data.push(value);
                }
            }
        });
        // 自动推断维度
        if (!dimensions && data.length > 0) {
            dimensions = Object.keys(data[0]);
        }
        this.recursiveSphereWeaving = new RecursiveSphereWeaving(data, this.riemannSphereDB, dimensions);
        console.log("Recursive sphere weaving created");
    }

    queryRecursiveSphereWeaving(queryPoint, k = 5) {
        if (!this.recursiveSphereWeaving) {
            console.log("Recursive sphere weaving not created");
            return;
        }
        const results = this.recursiveSphereWeaving.query(queryPoint, k);
        console.log(results);
        return results;
    }
    cloneTable(sourceTable, targetTable) {
        try {
            if (!this.currentDatabase) {
                console.log("No database selected");
                return;
            }
            if (!this.data.has(this.currentDatabase) || !this.data.get(this.currentDatabase).has(sourceTable)) {
                console.log(`Source table ${sourceTable} does not exist`);
                return;
            }
            if (this.data.get(this.currentDatabase).has(targetTable)) {
                console.log(`Target table ${targetTable} already exists`);
                return;
            }

            const sourceTableData = this.data.get(this.currentDatabase).get(sourceTable);
            const clonedTableData = JSON.parse(JSON.stringify(sourceTableData));
            this.data.get(this.currentDatabase).set(targetTable, clonedTableData);
            // 保存黎曼球面映射数据
            //this.riemannSphereDB.saveMappedData();
            this.saveData();
            console.log(`Table ${sourceTable} cloned to ${targetTable}`);
            log('INFO', `Table ${sourceTable} cloned to ${targetTable} successfully`);
        } catch (error) {
            handleError(error, 'cloning table');
        }
    }

    cloneDatabase(sourceDatabase, targetDatabase) {
        try {
            if (!this.data.has(sourceDatabase)) {
                console.log(`Source database ${sourceDatabase} does not exist`);
                return;
            }
            if (this.data.has(targetDatabase)) {
                console.log(`Target database ${targetDatabase} already exists`);
                return;
            }

            const sourceDatabaseData = this.data.get(sourceDatabase);
            const clonedDatabaseData = new Map();
            sourceDatabaseData.forEach((tableData, tableName) => {
                clonedDatabaseData.set(tableName, JSON.parse(JSON.stringify(tableData)));
            });
            this.data.set(targetDatabase, clonedDatabaseData);
            // 保存黎曼球面映射数据
            //this.riemannSphereDB.saveMappedData();
            this.saveData();
            console.log(`Database ${sourceDatabase} cloned to ${targetDatabase}`);
            log('INFO', `Database ${sourceDatabase} cloned to ${targetDatabase} successfully`);
        } catch (error) {
            handleError(error, 'cloning database');
        }
    }
    registerExtension(extension) {
        if (extension.name && typeof extension.execute === 'function') {
            this.extensions[extension.name] = extension;
            log('INFO', `Extension '${extension.name}' registered`);
        } else {
            console.log("Invalid extension: must have a 'name' property and an 'execute' function");
        }
    }
    // 添加压缩禁用方法
    disableCompression(tableName) {
        if (!this.currentDatabase) {
            console.log("No database selected");
            return;
        }

        const db = this.data.get(this.currentDatabase);
        if (!db || !db.has(tableName)) {
            console.log("Table does not exist");
            return;
        }

        const table = db.get(tableName);
        if (table instanceof PartitionedTable) {
            table.compressed = false;
            this.compressionDisabled = true;
            console.log(`Compression disabled for table ${tableName}`);
        }
    }
    setnullcomposite(tableName) {
        // 销毁复合索引（DimensionsTree）
        if (this.indexes && this.indexes.has(tableName)) {
            // 只删除 DimensionsTree 类型的索引
            for (const [key, indexObj] of this.indexes.get(tableName).entries()) {
                if (indexObj instanceof DimensionsTree) {
                    this.indexes.get(tableName).delete(key);
                }
            }
        }
        this.dimensiontree = null;
        console.log(`Composite index for table ${tableName} destroyed`);
    };

    setnullpyramid() {
        // 销毁金字塔索引
        this.pyramidIndex = null;
        console.log("Pyramid index destroyed");
    };

    setnullrecursive() {
        // 销毁递归球面编织索引
        this.recursiveSphereWeaving = null;
        console.log("Recursive sphere weaving index destroyed");

    };
    // 删除第3183行的孤立 { 

    // 修复 K-means 聚类方法 - 完整实现
    async kMeansClustering(tableName, dimensions, k, maxIterations = 100) {
        try {
            if (!this.currentDatabase) {
                console.log("No database selected");
                return;
            }
            if (!this.data.has(this.currentDatabase) || !this.data.get(this.currentDatabase).has(tableName)) {
                console.log("Table does not exist");
                return;
            }

            const partitionedTable = this.data.get(this.currentDatabase).get(tableName);
            let allData = [];

            // 从分区表中收集所有数据
            partitionedTable.partitions.forEach(partition => {
                if (partition) {
                    for (const data of partition.values()) {
                        allData.push(data);
                    }
                }
            });

            // 提取指定维度的数值数据
            const dataPoints = allData
                .map(data => {
                    const point = dimensions.map(dim => {
                        const val = data[dim];
                        return (val !== undefined && !isNaN(val)) ? parseFloat(val) : 0;
                    });
                    return { original: data, point };
                })
                .filter(item => item.point.some(val => !isNaN(val)));

            if (dataPoints.length === 0) {
                console.log("No valid numeric data points to cluster");
                return;
            }

            if (k > dataPoints.length) {
                console.log(`k (${k}) cannot be greater than number of data points (${dataPoints.length})`);
                return;
            }

            // 执行聚类
            const points = dataPoints.map(item => item.point);
            const result = this.performKMeans(points, k, maxIterations);

            // 更新原始数据的聚类标签
            result.clusters.forEach((cluster, clusterIndex) => {
                cluster.forEach(pointIndex => {
                    if (pointIndex < dataPoints.length) {
                        dataPoints[pointIndex].original.cluster_label = clusterIndex;
                    }
                });
            });

            this.saveData();
            console.log(`K-means clustering completed with ${k} clusters`);
            console.log(`Centroids:`, result.centroids);
            log('INFO', 'K-means clustering completed successfully');

            return result;
        } catch (error) {
            handleError(error, 'k-means clustering');
        }
    }

    performKMeans(points, k, maxIterations = 100) {
        if (points.length === 0 || k <= 0) {
            return { centroids: [], clusters: [] };
        }

        // 初始化质心
        let centroids = this.initializeCentroids(points, k);
        let clusters = [];

        for (let iteration = 0; iteration < maxIterations; iteration++) {
            // 分配点到最近的质心
            clusters = this.assignClusters(points, centroids);

            // 计算新质心
            const newCentroids = this.calculateCentroids(clusters, points);

            // 检查收敛
            if (this.haveConverged(centroids, newCentroids)) {
                log('INFO', `K-means converged after ${iteration + 1} iterations`);
                break;
            }

            centroids = newCentroids;
        }

        return { centroids, clusters };
    }

}
class PyramidIndex {
    constructor(data, maxCapacity = 2, k = 5) {
        this.data = Array.isArray(data) ? data : [];
        this.maxCapacity = maxCapacity;
        this.k = k;
        // 自动推断维度数量
        this.dimensions = this.data.length > 0
            ? (Array.isArray(this.data[0]) ? this.data[0].length : Object.values(this.data[0]).filter(v => typeof v === 'number' || !isNaN(parseFloat(v))).length)
            : 0;
        this.partitions = [];

        if (this.data.length > maxCapacity) {
            this.partitions = this._recursivePartition(this.data, 0);
        } else {
            this.partitions = [this.data];
        }
    }

    // 递归分区方法，支持任意维度
    _recursivePartition(data, depth) {
        if (data.length <= this.maxCapacity || this.dimensions === 0) {
            return [data];
        }
        const dim = depth % this.dimensions;
        // 提取该维度的所有值（过滤字符串，后面补median）
        let values = data.map(item =>
            Array.isArray(item)
                ? item[dim]
                : Object.values(item).filter(v => typeof v === 'number' || !isNaN(parseFloat(v)))[dim]
        ).filter(v => typeof v !== 'string' && v !== undefined && !isNaN(v));
        if (values.length === 0) { return [data]; }

        // 计算中位数
        const sorted = [...values].sort((a, b) => a - b);
        const median = sorted[Math.floor(sorted.length / 2)];

        // 按当前维度中位数分割
        const left = [];
        const right = [];
        data.forEach(item => {
            let val = Array.isArray(item)
                ? item[dim]
                : Object.values(item).filter(v => typeof v === 'number' || !isNaN(parseFloat(v)))[dim];
            // 检查字符串
            if (typeof val === 'string' || val === undefined || isNaN(val)) {
                val = median; // 字符串或无效值视为median
            }
            if (val < median) { left.push(item); }
            else { right.push(item); }
        });

        return [
            ...this._recursivePartition(left, depth + 1),
            ...this._recursivePartition(right, depth + 1)
        ];
    }

    static euclideanDistance(a, b) {
        if (!Array.isArray(a) || !Array.isArray(b)) { return Infinity; }
        let sum = 0;
        for (let i = 0; i < Math.min(a.length, b.length); i++) {
            let va = a[i], vb = b[i];
            if (typeof va === 'string' || va === undefined || isNaN(va)) { va = 0; }
            if (typeof vb === 'string' || vb === undefined || isNaN(vb)) { vb = 0; }
            va = parseFloat(va);
            vb = parseFloat(vb);
            sum += Math.pow(va - vb, 2);
        }
        return Math.sqrt(sum);
    }

    // 递归查找目标分区路径，支持回溯
    _findPartitionPath(partitions, queryPoint, depth = 0, k = 5) {
        if (!Array.isArray(partitions) || partitions.length === 0) { return []; }
        if (!Array.isArray(partitions[0])) { return [partitions]; } // 到达叶子分区

        const dim = depth % this.dimensions;
        // 只在当前层选一个目标分区
        let minDist = Infinity, targetIdx = 0;
        partitions.forEach((sub, idx) => {
            if (sub.length === 0) { return; }
            let center = Array.isArray(sub[0])
                ? sub[0]
                : Object.values(sub[0]).filter(v => typeof v === 'number' || !isNaN(parseFloat(v)));
            const dist = PyramidIndex.euclideanDistance(center, queryPoint);
            if (dist < minDist) {
                minDist = dist;
                targetIdx = idx;
            }
        });

        const subPartition = partitions[targetIdx];
        if (!Array.isArray(subPartition[0])) {
            // 到达叶子，收集相邻分区
            const leaves = [];
            leaves.push(subPartition);
            if (targetIdx > 0) { leaves.push(partitions[targetIdx - 1]); }
            if (targetIdx < partitions.length - 1) { leaves.push(partitions[targetIdx + 1]); }
            // 检查候选点数量
            const totalCount = leaves.reduce((sum, arr) => sum + arr.length, 0);
            if (totalCount >= k || depth === 0) {
                return leaves;
            } else {
                // 回溯上一层，扩大候选范围
                return partitions;
            }
        } else {
            // 还没到叶子，继续递归目标分区
            return this._findPartitionPath(subPartition, queryPoint, depth + 1, k);
        }
    }

    // 插入
    insert(item) {
        this.data.push(item);
        if (this.data.length > this.maxCapacity) {
            this.partitions = this._recursivePartition(this.data, 0);
        } else {
            this.partitions = [this.data];
        }
    }

    // 删除
    delete(item) {
        this.data = this.data.filter(d => !this._isSame(d, item));
        if (this.data.length > this.maxCapacity) {
            this.partitions = this._recursivePartition(this.data, 0);
        } else {
            this.partitions = [this.data];
        }
    }

    // 更新
    update(oldItem, newItem) {
        this.delete(oldItem);
        this.insert(newItem);
    }

    // 查询最近的k个点（保证返回数量稳定）
    query(queryPoint) {
        if (!Array.isArray(queryPoint)) {
            console.log("Query point must be an array");
            return [];
        }
        // 递归查找目标路径，传入k
        let candidatePartitions = this._findPartitionPath(this.partitions, queryPoint, 0, this.k);
        let candidates = candidatePartitions.flat();

        // 如果候选点数量仍然不足k，直接全量遍历
        if (candidates.length < this.k) {
            candidates = this.data;
        }

        // 只在候选点中查找最近k个
        const results = candidates
            .map(item => {
                let pointArr = Array.isArray(item)
                    ? item
                    : Object.values(item).filter(v => typeof v === 'number' || !isNaN(parseFloat(v)));
                return {
                    item,
                    dist: PyramidIndex.euclideanDistance(pointArr, queryPoint)
                };
            })
            .sort((a, b) => a.dist - b.dist)
            .slice(0, this.k)
            .map(r => r.item);
        return results;
    }


    // 判断两个对象/数组是否相同（用于删除）
    _isSame(a, b) {
        if (Array.isArray(a) && Array.isArray(b)) {
            if (a.length !== b.length) return false;
            for (let i = 0; i < a.length; i++) {
                if (a[i] !== b[i]) return false;
            }
            return true;
        } else if (typeof a === 'object' && typeof b === 'object') {
            const keysA = Object.keys(a);
            const keysB = Object.keys(b);
            if (keysA.length !== keysB.length) return false;
            for (let key of keysA) {
                if (a[key] !== b[key]) return false;
            }
            return true;
        } else {
            return a === b;
        }
    }
}
// 递归球面编织结构体
class RecursiveSphereWeaving {
    constructor(data, riemannSphereDB, dimensions = null) {
        this.data = data;
        this.riemannSphereDB = riemannSphereDB;
        this.dimensions = dimensions || (data.length > 0 ? Object.keys(data[0]) : []);
        // 预先将所有数据点映射到球面空间
        this.spherePoints = data.map(row => {
            // 字符串字段视为0
            const vec = this.dimensions.map(dim => {
                const v = row[dim];
                return (typeof v === 'string' || v === undefined || isNaN(v)) ? 0 : parseFloat(v);
            });
            return {
                original: row,
                sphere: this.riemannSphereDB.mapToRiemannSphere(vec)
            };
        });
        this.euclideanDistance = hypercubeDB.euclideanDistance;
    }
    // 在 RecursiveSphereWeaving 类中添加
    insert(row) {
        const vec = this.dimensions.map(dim => {
            const v = row[dim];
            return (typeof v === 'string' || v === undefined || isNaN(v)) ? 0 : parseFloat(v);
        });
        this.spherePoints.push({
            original: row,
            sphere: this.riemannSphereDB.mapToRiemannSphere(vec)
        });
    }
    delete(row) {
        this.spherePoints = this.spherePoints.filter(pt => pt.original !== row);
    }
    update(oldRow, newRow) {
        this.delete(oldRow);
        this.insert(newRow);
    }
    // 查询：输入高维点，返回球面空间最近的若干原始数据
    query(queryPoint, k = 5) {
        // queryPoint: 高维向量或对象
        let vec;
        if (Array.isArray(queryPoint)) {
            vec = queryPoint;
        } else if (typeof queryPoint === 'object') {
            vec = this.dimensions.map(dim => parseFloat(queryPoint[dim]) || 0);
        } else {
            throw new Error('Invalid query point');
        }
        const sphereQuery = this.riemannSphereDB.mapToRiemannSphere(vec);

        // 计算球面空间距离，返回最近的k个
        const results = this.spherePoints
            .map(item => ({
                original: item.original,
                dist: this.euclideanDistance(item.sphere, sphereQuery)
            }))
            .sort((a, b) => a.dist - b.dist)
            .slice(0, k)
            .map(r => r.original);

        return results;
    }
}
class RiemannSphereDB {
    constructor() {
        this.data = new Map();
        this.mappedData = new Map();
        this.sphereCenter = [0, 0, 0];
        this.compressionRate = 0.7;
        this.spatialIndex = new Map();
    }
    // 只保留这一个实现
    mapToRiemannSphere(dataPoint) {
        // dataPoint 应为数组
        if (!Array.isArray(dataPoint) || dataPoint.length < 2) { return [0, 0, -1]; }
        // 字符串字段视为0
        const v0 = (typeof dataPoint[0] === 'string' || dataPoint[0] === undefined || isNaN(dataPoint[0])) ? 0 : parseFloat(dataPoint[0]);
        const v1 = (typeof dataPoint[1] === 'string' || dataPoint[1] === undefined || isNaN(dataPoint[1])) ? 0 : parseFloat(dataPoint[1]);
        const theta = (Math.atan(v0) * 180) / Math.PI;
        const phi = (Math.atan(v1) * 180) / Math.PI;
        const thetaRad = (theta * Math.PI) / 180;
        const phiRad = (phi * Math.PI) / 180;
        const r = 1;
        const x = r * Math.cos(thetaRad) * Math.cos(phiRad);
        const y = r * Math.cos(thetaRad) * Math.sin(phiRad);
        const z = r * Math.sin(thetaRad);
        return [x, y, z];
    }



    buildSpatialIndex() {
        this.spatialIndex.clear();
        this.mappedData.forEach((mappedPoint, key) => {
            const gridKey = mappedPoint.slice(0, 3).map(v => Math.floor(v * 10)).join('_');
            if (!this.spatialIndex.has(gridKey)) {
                this.spatialIndex.set(gridKey, []);
            }
            this.spatialIndex.get(gridKey).push(key);
        });
    }

    insertData(dataPoint) {
        const key = this.hashDataPoint(dataPoint);
        this.data[key] = dataPoint;
        this.mappedData[key] = this.mapToRiemannSphere(dataPoint);
    }

    hashDataPoint(dataPoint) {
        const crypto = require('crypto');
        return crypto.createHash('sha256').update(JSON.stringify(dataPoint)).digest('hex');
    }

    selectData(tableName, query) {
        try {
            if (!this.currentDatabase) {
                console.log("No database selected");
                return;
            }
            const currentDB = this.data.get(this.currentDatabase);
            if (!currentDB) {
                console.log("Database does not exist");
                return;
            }
            if (!currentDB.has(tableName)) {
                console.log("Table does not exist");
                return;
            }
            if (this.acquireLock(tableName)) {
                try {
                    let results = this.data[this.currentDatabase][tableName];

                    // 使用距离哈希加速查询
                    if (query.distance !== undefined) {
                        const distance = query.distance;
                        const bucketSize = 0.1; // 每个哈希桶的大小
                        const bucketKey = Math.floor(distance / bucketSize) * bucketSize;

                        if (this.distanceHashes[tableName][bucketKey]) {
                            results = this.distanceHashes[tableName][bucketKey].filter(data => {
                                const pointDistance = Math.sqrt(Object.keys(data).reduce((sum, dim) => sum + Math.pow(data[dim], 2), 0));
                                return pointDistance >= distance - bucketSize && pointDistance < distance + bucketSize;
                            });
                        } else {
                            results = [];
                        }
                    } else {
                        // 使用索引进行查询
                        const indexKeys = Object.keys(this.indexes[tableName]).filter(indexKey => {
                            return indexKey.split('_').every(dim => query[dim] !== undefined);
                        });

                        if (indexKeys.length > 0) {
                            const indexKey = indexKeys[0];
                            const indexValues = indexKey.split('_').map(dim => query[dim]);
                            const indexValue = indexValues.join('_');
                            results = this.indexes[tableName][indexKey][indexValue] || [];
                        } else {
                            // 解析查询条件
                            const filterFunction = this.parseQuery(query);
                            results = results.filter(filterFunction);
                        }
                    }

                    // 根据聚类标签查询
                    if (query.label !== undefined) {
                        results = results.filter(data => data.label === query.label);
                    }

                    console.log(results);
                } finally {
                    this.releaseLock(tableName);
                }
            } else {
                console.log("Table is locked");
            }
            log('INFO', 'Data selected successfully');
        } catch (error) {
            handleError(error, 'selecting data');
        }
    }

    isMatch(data, query) {
        try {
            if (!query || Object.keys(query).length === 0) {
                return true;
            }

            return Object.entries(query).every(([field, condition]) => {
                console.log(condition);
                // 处理嵌套查询条件
                if (typeof condition === 'object' && condition !== null) {

                    // 检查是否包含子查询
                    if (condition.hasOwnProperty('_subquery')) {
                        const subQueryResult = this._executeSubQuery(condition._subquery);

                        // 验证子查询结果
                        if (!Array.isArray(subQueryResult) || subQueryResult.length !== 1) {
                            throw new Error('Subquery must return exactly one row');
                        }

                        // 获取子查询结果的指定列值
                        const columnValue = subQueryResult[0][condition._column];
                        if (columnValue === undefined) {
                            throw new Error(`Column ${condition._column} not found in subquery result`);
                        }
                        console.log('/n');
                        console.log(columnValue);
                        console.log('/n');
                        return data[field] === columnValue;

                    }

                    // 处理复合条件
                    if (condition._op) {
                        switch (condition._op.toLowerCase()) {
                            case 'in':
                                return Array.isArray(condition._value) &&
                                    condition._value.includes(data[field]);
                            case 'between':
                                return Array.isArray(condition._value) &&
                                    condition._value.length === 2 &&
                                    data[field] >= condition._value[0] &&
                                    data[field] <= condition._value[1];
                            case 'like':
                                const pattern = new RegExp(
                                    condition._value.replace(/%/g, '.*').replace(/_/g, '.'),
                                    'i'
                                );
                                return pattern.test(String(data[field]));
                            default:
                                throw new Error(`Unsupported operator: ${condition._op}`);
                        }
                    }

                    // 处理列引用
                    if (condition._column && condition._table) {
                        const refValue = this._getReferencedValue(
                            condition._table,
                            condition._column,
                            data
                        );
                        return data[field] === refValue;
                    }
                }

                // 处理简单条件（直接值比较）
                return data[field] === condition;
            });
        } catch (error) {
            log('ERROR', `Error in isMatch: ${error.message}`);
            return false;
        }
    }

    // 执行子查询
    _executeSubQuery(subquery) {
        try {
            // 解析并执行子查询
            const result = this.query(subquery);

            if (!Array.isArray(result)) {
                throw new Error('Subquery execution failed');
            }

            return result;
        } catch (error) {
            log('ERROR', `Subquery execution failed: ${error.message}`);
            throw error;
        }
    }

    // 获取引用列的值
    _getReferencedValue(tableName, columnName, currentRow) {
        try {
            // 查找引用表
            const refTable = this._findTable(tableName);
            if (!refTable) {
                throw new Error(`Referenced table ${tableName} not found`);
            }

            // 执行关联查询
            const result = refTable.query({
                [columnName]: currentRow[columnName]
            });

            if (!Array.isArray(result) || result.length !== 1) {
                throw new Error(`Invalid reference result for ${tableName}.${columnName}`);
            }

            return result[0][columnName];
        } catch (error) {
            log('ERROR', `Reference resolution failed: ${error.message}`);
            throw error;
        }
    }
    riemannKMeansClustering(k) {
        const lowDimPoints = Object.values(this.mappedData);
        const clusters = this.performKMeans(lowDimPoints, k);

        // 将聚类结果映射回高维空间
        const highDimClusters = clusters.map(cluster => {
            return cluster.map(mappedPoint => {
                const key = this.hashDataPoint(mappedPoint);
                return this.data[key];
            });
        });

        return highDimClusters;
    }


    performKMeans(points, k, maxIterations = 100) {
        if (points.length === 0 || k <= 0) {
            return [];
        }

        // 1. 随机初始化K个质心
        let centroids = this.initializeCentroids(points, k);

        let clusters = [];
        let previousClusters = [];

        for (let iteration = 0; iteration < maxIterations; iteration++) {
            // 2. 将每个数据点分配到最近的质心
            clusters = this.assignClusters(points, centroids);

            // 3. 更新质心为每个簇的平均值
            const newCentroids = this.calculateCentroids(clusters, points);

            // 4. 检查是否收敛
            if (this.haveConverged(centroids, newCentroids)) {
                log('INFO', `K-means converged after ${iteration + 1} iterations`);
                break;
            }

            centroids = newCentroids;
            previousClusters = clusters;
        }

        return clusters;
    }

    visualizeData() {
        const points = Object.values(this.mappedData);
        const visualizationData = points.map(point => ({
            x: point[0],
            y: point[1],
            z: point[2], // 假设映射到三维空间
        }));

        return visualizationData;
    }
}
class DimensionsTree {
    constructor(dimensions) {
        this.root = new Map(); // 树的根节点
        this.dimensions = dimensions; // 索引的维度
    }

    // 插入数据点
    insert(keys, value, node = this.root, depth = 0) {
        if (depth === this.dimensions.length) {
            if (!node.has('_data')) { node.set('_data', []); }
            node.get('_data').push(value);
            return;
        }
        // 强制转字符串
        const key = String(keys[depth]);
        if (!node.has(key)) { node.set(key, new Map()); }
        this.insert(keys, value, node.get(key), depth + 1);
    }

    // 查询数据点
    query(range, node = this.root, depth = 0, results = []) {
        if (depth === this.dimensions.length) {
            if (node.has('_data')) { results.push(...node.get('_data')); }
            return results;
        }
        // 强制转字符串
        const [min, max] = range[depth];
        const minStr = String(min);
        const maxStr = String(max);
        for (const [key, childNode] of node.entries()) {
            if (key == '_data') { continue; }
            if (key >= minStr && key <= maxStr) {
                this.query(range, childNode, depth + 1, results);
            }
        }
        return results;
    }
    // 在 DimensionsTree 类中添加
    delete(keys, value, node = this.root, depth = 0) {
        if (depth === this.dimensions.length) {
            if (node.has('_data')) {
                node.set('_data', node.get('_data').filter(v => v !== value));
            }
            return;
        }
        const key = String(keys[depth]);
        if (node.has(key)) {
            this.delete(keys, value, node.get(key), depth + 1);
        }
    }
    update(keys, oldValue, newValue, node = this.root, depth = 0) {
        if (depth === this.dimensions.length) {
            if (node.has('_data')) {
                const data = node.get('_data');
                const index = data.indexOf(oldValue);
                if (index !== -1) {
                    data[index] = newValue;
                }
            }
            return;
        }
        const key = String(keys[depth]);
        if (node.has(key)) {
            this.update(keys, oldValue, newValue, node.get(key), depth + 1);
        }
    }
}
const { Worker, isMainThread, parentPort, workerData } = require('worker_threads');
const { stringify } = require('querystring');

// K-means 聚类计算逻辑（Worker 线程）
function computeClusters(dataPoints, k, maxIterations) {
    function euclideanDistance(a, b) {
        return Math.sqrt(a.reduce((sum, val, i) => sum + (val - b[i]) ** 2, 0));
    }

    function initializeCentroids(dataPoints, k) {
        const indices = Array.from({ length: dataPoints.length }, (_, i) => i);
        const randomIndices = indices.sort(() => Math.random() - 0.5).slice(0, k);
        return randomIndices.map(index => dataPoints[index]);
    }

    function assignClusters(dataPoints, centroids) {
        const clusters = Array.from({ length: k }, () => []);
        dataPoints.forEach((point, index) => {
            const distances = centroids.map(centroid => euclideanDistance(point, centroid));
            const closestCentroidIndex = distances.indexOf(Math.min(...distances));
            clusters[closestCentroidIndex].push(index);
        });
        return clusters;
    }

    function calculateCentroids(clusters, dataPoints) {
        return clusters.map(cluster => {
            if (cluster.length === 0) { return dataPoints[Math.floor(Math.random() * dataPoints.length)]; }
            const sum = cluster.reduce((acc, index) => {
                return acc.map((val, i) => val + dataPoints[index][i]);
            }, Array(dataPoints[0].length).fill(0));
            return sum.map(val => val / cluster.length);
        });
    }

    function haveConverged(oldCentroids, newCentroids) {
        return oldCentroids.every((oldCentroid, index) => {
            return euclideanDistance(oldCentroid, newCentroids[index]) < 1e-5;
        });
    }

    let centroids = initializeCentroids(dataPoints, k);
    let clusters = [];

    for (let iteration = 0; iteration < maxIterations; iteration++) {
        clusters = assignClusters(dataPoints, centroids);
        const newCentroids = calculateCentroids(clusters, dataPoints);

        if (haveConverged(centroids, newCentroids)) {
            break;
        }

        centroids = newCentroids;
    }

    return { centroids, clusters };
}
// executeScriptFile 函数
async function executeScriptFile(filePath) {
    try {
        const data = fs.readFileSync(filePath, 'utf8');
        const lines = data.split('\n');

        for (const line of lines) {
            const trimmedLine = line.trim();
            if (trimmedLine && !trimmedLine.startsWith('#')) { // 忽略空行和注释行
                executePrompt(trimmedLine);
            }
        }
        console.log(`Script file '${filePath}' executed successfully`);
        log('INFO', `Script file '${filePath}' executed successfully`);
    } catch (error) {
        handleError(error, 'executing script file');
    }
}


function parseConditions(conditionStr) {
    if (typeof conditionStr !== 'string') {
        throw new Error('Condition must be a string');
    }

    // 处理 AND 条件
    if (conditionStr.toLowerCase().includes(' and ')) {
        return {
            _op: 'AND',
            _conditions: conditionStr.split(/\s+and\s+/i).map(c => parseConditions(c))
        };
    }

    // 处理 OR 条件
    if (conditionStr.toLowerCase().includes(' or ')) {
        return {
            _op: 'OR',
            _conditions: conditionStr.split(/\s+or\s+/i).map(c => parseConditions(c))
        };
    }

    // 处理 NOT 条件
    if (conditionStr.toLowerCase().startsWith('not ')) {
        return {
            _op: 'NOT',
            _condition: parseConditions(conditionStr.substring(4))
        };
    }

    // 处理子查询
    if (conditionStr.includes('{') && conditionStr.includes('}')) {
        const subqueryMatch = /^([a-zA-Z0-9_]+)\s*=\s*\{([^}]+)\}$/.exec(conditionStr);
        if (subqueryMatch) {
            const [_, field, subqueryParts] = subqueryMatch;
            const [column, subquery] = subqueryParts.split(',').map(s => s.trim());
            return {
                [field]: {
                    column: column.split(':')[1],
                    subquery: subquery.split(':')[1]
                }
            };
        }
    }

    // 处理基本条件
    const operatorPattern = /^([a-zA-Z0-9_]+)\s*(=|>|<|>=|<=|<>)\s*([^=<>]+)$/;
    const match = operatorPattern.exec(conditionStr.trim());

    if (!match) {
        throw new Error('Invalid condition format');
    }

    const [_, field, operator, value] = match;
    return parseBasicCondition(field, operator, value.trim());
}

function parseBasicCondition(field, operator, value) {
    if (!field || !operator) {
        throw new Error('Missing field name or operator');
    }

    // 支持的操作符
    const validOperators = ['=', '>', '<', '>=', '<=', '<>'];

    // 验证操作符
    if (!validOperators.includes(operator)) {
        throw new Error(`Invalid operator: ${operator}`);
    }

    // 处理值
    let parsedValue;
    if (value === undefined || value === null) {
        throw new Error(`Missing value for condition: ${field} ${operator}`);
    }

    // 处理数值和字符串
    parsedValue = value.startsWith('"') || value.startsWith("'")
        ? value.slice(1, -1)
        : isNaN(value) ? value : Number(value);

    return {
        [field]: {
            _op: operator,
            _value: parsedValue
        }
    };
}
// Worker 线程入口
if (!isMainThread) {
    const { dataPoints, k, maxIterations } = workerData;
    const result = computeClusters(dataPoints, k, maxIterations);
    parentPort.postMessage(result);
}
function factorial(n) {
    if (n === 0 || n === 1) { return 1; }
    return n * factorial(n - 1);
}
// ...existing code...
// 修复数据点生成函数
function generateDataPoints(distributionType, params) {
    const dataPoints = [];
    const numPoints = params.numPoints || 100;

    try {
        switch (distributionType.toLowerCase()) {
            case 'uniform':
                for (let i = 0; i < numPoints; i++) {
                    const point = {};
                    Object.keys(params).forEach(key => {
                        if (key !== 'numPoints' && params[key].min !== undefined && params[key].max !== undefined) {
                            point[key] = Math.random() * (params[key].max - params[key].min) + params[key].min;
                        }
                    });
                    if (Object.keys(point).length > 0) {
                        point.id = i; // 添加ID字段
                        dataPoints.push(point);
                    }
                }
                break;

            case 'normal':
                for (let i = 0; i < numPoints; i++) {
                    const point = {};
                    Object.keys(params).forEach(key => {
                        if (key !== 'numPoints' && params[key].mean !== undefined) {
                            const mean = params[key].mean || 0;
                            const stdDev = params[key].stdDev || 1;
                            point[key] = normalRandom(mean, stdDev);
                        }
                    });
                    if (Object.keys(point).length > 0) {
                        point.id = i;
                        dataPoints.push(point);
                    }
                }
                break;

            case 'sphere':
                const radius = params.radius || 1;
                for (let i = 0; i < numPoints; i++) {
                    const theta = Math.random() * 2 * Math.PI;
                    const phi = Math.acos(2 * Math.random() - 1);
                    const point = {
                        id: i,
                        x: radius * Math.sin(phi) * Math.cos(theta),
                        y: radius * Math.sin(phi) * Math.sin(theta),
                        z: radius * Math.cos(phi)
                    };
                    dataPoints.push(point);
                }
                break;

            default:
                console.log(`Unsupported distribution type: ${distributionType}`);
                return [];
        }
    } catch (error) {
        console.log(`Error generating points: ${error.message}`);
        return [];
    }

    return dataPoints;
}
// ...existing code...


// 正态分布随机数生成器
function normalRandom(mean = 0, stdDev = 1) {
    let u = 0, v = 0;
    while (u === 0) { u = Math.random(); } // 避免log(0)
    while (v === 0) { v = Math.random(); }
    const z = Math.sqrt(-2.0 * Math.log(u)) * Math.cos(2.0 * Math.PI * v);
    return z * stdDev + mean;
}
const db = new HypercubeDB();
global.hypercubeDB = db;
function displayPrompt() {
    process.stdout.write('>');
}
function executePrompt(answer) {
    const reg1 = /^\s*create\s+database\s+([a-zA-Z0-9_]+)\s*$/;
    const reg2 = /^\s*use\s+([a-zA-Z0-9_]+)\s*$/;
    const reg3 = /^\s*create\s+table\s+([a-zA-Z0-9_]+)\s*\(([a-zA-Z0-9_]+(,[a-zA-Z0-9_]+)*)\)\s*(partition\s+by\s+([a-zA-Z0-9_]+))?\s*(partitions\s*=\s*(\d+))?\s*$/;
    const reg4 = /^\s*insert\s+into\s+([a-zA-Z0-9_]+)\s+values\s*\{([^}]+)\}\s*$/;
    const reg5 = /^\s*insert\s+batch\s+into\s+([a-zA-Z0-9_]+)\s+values\s*\[([^\]]+)\]\s*$/;
    const reg6 = /^\s*exit\s*$/;
    const reg7 = /^\s*add\s+dimension\s+([a-zA-Z0-9_]+)\s+to\s+([a-zA-Z0-9_]+)\s*$/;
    const reg8 = /^\s*remove\s+dimension\s+([a-zA-Z0-9_]+)\s+from\s+([a-zA-Z0-9_]+)\s*$/;

    const reg9 = /^\s*get\s+fuzzy\s+function\s+([a-zA-Z0-9_]+)\s+on\s+([a-zA-Z0-9_]+)\s+and\s+([a-zA-Z0-9_]+)\s*$/;
    const reg10 = new RegExp("^\\s*taylor\\s+expand\\s+([a-zA-Z0-9_]+)\\s+on\\s+([a-zA-Z0-9_]+)\\s+and\\s+([a-zA-Z0-9_]+)\\s+at\\s+\\(([^,]+),([^)]+)\\)\\s+order\\s+(\\d+)\\s*$");

  //  const reg11 = new RegExp("^\\s*import\\s+csv\\s+([a-zA-Z0-9_]+)\\s+from\\s+([^\\s]+)\\s*$");

    const regUpdate = /^\s*update\s+([a-zA-Z0-9_]+)\s+set\s+([a-zA-Z0-9_]+)\s*=\s*([^\s]+)\s+where\s+(.+)\s*$/i;
    const regUpdateBatchColumn = /^\s*update\s+batch\s+column\s+([a-zA-Z0-9_]+)\s+set\s*\{([^\}]+)\}\s+where\s+(.+)\s*$/i;
    const regDelete = /^\s*delete\s+from\s+([a-zA-Z0-9_]+)\s+where\s+(.+)\s*$/i;
    const reg13 = new RegExp("^\\s*update\\s+batch\\s+([a-zA-Z0-9_]+)\\s+set\\s+\\[([^\]]+)\\]\\s*$");


    const reg15 = new RegExp("^\\s*drop\\s+table\\s+([a-zA-Z0-9_]+)\\s*$");
    const regSelectData = new RegExp(
        "^\\s*select\\s+\\*\\s+from\\s+([a-zA-Z0-9_]+)(?:\\s+where\\s+(.+))?$",
        "i"
    );
    const reg16 = /^\s*load\s+extension\s+([^\s]+)\s*$/i;
    const reg17 = /^\s*extension\s+(\S+)/i; // 精确匹配扩展名
    const reg33 = /^\s*remove\s+extension\s+(\S+)/i;
    const reg34 = /^\s*show\s+extensions\s*$/i;
    const reg18 = /^\s*create\s+composite\s+index\s+on\s+([a-zA-Z0-9_]+)\s+dimensions\s+([a-zA-Z0-9_,]+)\s*$/;
    const reg19 = /^\s*show\s+databases\s*$/i;

    const reg20 = /^\s*show\s+tables\s*$/i;
    const reg21 = /^\s*drop\s+database\s+([a-zA-Z0-9_]+)\s*$/i;
    const reg22 = /^\s*visualize\s+([\w]+)\s+with\s+([\w,]+)\s*$/;

    const reg23 = /^\s*insert\s+intersection\s+into\s+([a-zA-Z0-9_]+)\s+dimensions\s+([a-zA-Z0-9_,]+)\s+function\s+([^\s]+)\s+radius\s+(\d+(\.\d+)?)\s+resolution\s+(\d+(\.\d+)?)\s*$/;
    const reg24 = /^\s*kmeans\s+cluster\s+([a-zA-Z0-9_]+)\s+on\s+([a-zA-Z0-9_,]+)\s+with\s+k\s+(\d+)\s*$/;

    const reg25 = /^\s*create\s+pyramid\s+index\s+on\s+([a-zA-Z0-9_]+)\s+with\s+max_capacity=(\d+)\s+and\s+k=(\d+)\s*$/;
    const reg26 = /^\s*query\s+pyramid\s+index\s+on\s+([a-zA-Z0-9_]+)\s+with\s+point=\[([^\]]+)\]\s*$/;

    const reg27 = /^\s*create\s+recursive\s+sphere\s+weaving\s+on\s+([a-zA-Z0-9_]+)\s*$/;
    const reg28 = /^\s*query\s+recursive\s+sphere\s+weaving\s+on\s+([a-zA-Z0-9_]+)\s+with\s+point=\[([^\]]+)\]\s*$/;
    const regQueryRSW = /^\s*query\s+recursive\s+sphere\s+weaving\s+on\s+([a-zA-Z0-9_]+)\s+with\s+point=\[([^\]]*)\](?:with\s+k=(\d+))?\s*$/i;

    const reg29 = /^\s*execute\s+script\s+([^\s]+)\s*$/i; // 新增正则表达式来匹配 execute script 命令

    const regGeneratePoints = /^\s*generate\s+points\s+on\s+([a-zA-Z0-9_]+)\s+with\s+distribution\s+([a-zA-Z0-9_]+)\s+and\s+parameters\s*\{([^}]+)\}\s*$/;

    const regEnableCompression = /^\s*enable\s+compression\s+for\s+([a-zA-Z0-9_]+)\s*$/i;
    const regDisableCompression = /^\s*disable\s+compression\s+for\s+([a-zA-Z0-9_]+)\s*$/i;
    const regCloneTable = /^\s*clone\s+table\s+([a-zA-Z0-9_]+)\s+to\s+([a-zA-Z0-9_]+)\s*$/;
    const regCloneDatabase = /^\s*clone\s+database\s+([a-zA-Z0-9_]+)\s+to\s+([a-zA-Z0-9_]+)\s*$/;
    const regQueryCompositeIndex = /^\s*query\s+composite\s+index\s+on\s+([a-zA-Z0-9_]+)\s+with\s+dimensions\s*\{([^\}]+)\}\s*$/i;
    const regDestroyComposite = /^\s*destroy\s+composite\s+index\s+on\s+([a-zA-Z0-9_]+)\s*$/;
    const reg31 = /^\s*destroy\s+pyramid\s+index\s*$/;
    const reg32 = /^\s*destroy\s+recursive\s+sphere\s+weaving\s*$/;
    const regSelectFields = /^\s*select\s+([a-zA-Z0-9_,\s]+)\s+from\s+([a-zA-Z0-9_]+)(?:\s+where\s+(.+))?$/i;
    const reg35 = /^\s*--/; // 用于匹配注释行
    const regMount = /^\s*mount\s+([a-zA-Z0-9_]+)\s+from\s+([^\s]+)\s*$/i;
    const regDemount = /^\s*demount\s+([a-zA-Z0-9_]+)\s*$/i;
    const regloadAll = /^\s*load\s+all\s*$/i;



    if (reg1.test(answer)) {
        const dbName = reg1.exec(answer)[1];
        db.createDatabase(dbName);
    } else if (reg2.test(answer)) {
        const dbName = reg2.exec(answer)[1];
        db.useDatabase(dbName);
    } else if (reg3.test(answer)) {
        const match = reg3.exec(answer);
        if (match && match[2]) {
            const tableName = match[1];
            const schemaStr = match[2];
            const schema = schemaStr.split(',');
            const partitionKey = match[5] || 'id';
            const numPartitions = match[8] ? parseInt(match[8]) : 10; // 默认10
            db.createTable(tableName, schema, partitionKey, numPartitions);
        } else {
            console.log("Invalid command format for create table");
        }
    } else if (reg4.test(answer)) {
        const tableName = reg4.exec(answer)[1];
        const dataStr = reg4.exec(answer)[2];

        // 将键名转换为字符串形式
        const formattedDataStr = dataStr.replace(/(\w+):/g, '"$1":');

        try {
            const data = JSON.parse(`{${formattedDataStr}}`);
            db.insertData(tableName, data);
        } catch (error) {
            console.log("Invalid JSON format:", error.message);
        }
    } else if (reg5.test(answer)) {
        const tableName = reg5.exec(answer)[1];
        const dataArrayStr = reg5.exec(answer)[2];

        try {
            // 将键值对格式化为标准 JSON
            const formattedStr = dataArrayStr
                .replace(/([a-zA-Z0-9_]+):/g, '"$1":')  // 将 key: 转换为 "key":
                .replace(/'/g, '"');  // 将单引号替换为双引号

            // 尝试解析 JSON
            const dataArray = JSON.parse(`[${formattedStr}]`);
            db.insertBatch(tableName, dataArray);
        } catch (error) {
            console.log("Invalid JSON format:", error.message);
            console.log("提示: JSON格式需要键名使用双引号，例如: {\"id\":1,\"name\":\"Alice\"}");
        }
    } else if (reg6.test(answer)) {
        rl.close();
    } else if (reg7.test(answer)) {
        const dimension = reg7.exec(answer)[1];
        const tableName = reg7.exec(answer)[2];
        db.addDimension(tableName, dimension);
    } else if (reg8.test(answer)) {
        const dimension = reg8.exec(answer)[1];
        const tableName = reg8.exec(answer)[2];
        db.removeDimension(tableName, dimension);
    } else if (reg9.test(answer)) {
        const tableName = reg9.exec(answer)[1];
        const dim1 = reg9.exec(answer)[2];
        const dim2 = reg9.exec(answer)[3];
        const fuzzyFunction = db.getFuzzyFunction(tableName, dim1, dim2);
        console.log(`Fuzzy function on ${dim1} and ${dim2}:`, fuzzyFunction);
    } else if (reg10.test(answer)) {
        const tableName = reg10.exec(answer)[1];
        const dim1 = reg10.exec(answer)[2];
        const dim2 = reg10.exec(answer)[3];
        const x0 = parseFloat(reg10.exec(answer)[4]);
        const y0 = parseFloat(reg10.exec(answer)[5]);
        const order = parseInt(reg10.exec(answer)[6]);
        const terms = db.taylorExpand(tableName, dim1, dim2, x0, y0, order);
        console.log(`Taylor expansion of fuzzy function on ${dim1} and ${dim2} at (${x0}, ${y0}) order ${order}:`, terms);
    } /*else if (reg11.test(answer)) {
        const tableName = reg11.exec(answer)[1];
        const filePath = reg11.exec(answer)[2];
        db.importFromCSV(tableName, filePath);
    }*/ // 在executePrompt函数中更新处理逻辑
    else if (regUpdate.test(answer)) {
        const match = regUpdate.exec(answer);
        const tableName = match[1];
        const column = match[2];
        const newValue = match[3];
        const conditionStr = match[4];

        try {
            const query = parseConditions(conditionStr);
            const newData = { [column]: isNaN(newValue) ? newValue : parseFloat(newValue) };
            db.updateData(tableName, query, newData);
        } catch (error) {
            console.log(error.message);
        }
    }// executePrompt中的分支
    else if (regUpdateBatchColumn.test(answer)) {
        const match = regUpdateBatchColumn.exec(answer);
        const tableName = match[1];
        const columnsStr = match[2];
        const conditionStr = match[3];

        // 解析字段和值
        let newData = {};
        columnsStr.split(',').forEach(pair => {
            const [col, val] = pair.split('=').map(s => s.trim());
            newData[col] = isNaN(val) ? val : parseFloat(val);
        });

        // 解析条件
        let query = {};
        try {
            query = parseConditions(conditionStr);
        } catch (error) {
            console.log(error.message);
            return;
        }

        db.updateBatchColumn(tableName, query, newData);
    }
    else if (regDelete.test(answer)) {
        const match = regDelete.exec(answer);
        const tableName = match[1];
        const conditionStr = match[2];

        try {
            const query = parseConditions(conditionStr);
            db.deleteData(tableName, query);
        } catch (error) {
            console.log(error.message);
        }
    } // executePrompt中的 updateBatch 分支
    else if (reg13.test(answer)) {
        const match = reg13.exec(answer);
        const tableName = match[1];
        const updatesStr = match[2];
        try {
            // 支持格式：[{"key":主键值,"newData":{"字段1":值1,"字段2":值2}},...]
            const updates = JSON.parse(`[${updatesStr}]`);
            db.updateBatch(tableName, updates);
        } catch (error) {
            console.log("Invalid JSON format:", error.message);
        }
    } else if (reg15.test(answer)) {
        const tableName = reg15.exec(answer)[1];
        db.dropTable(tableName);
    } else if (regSelectData.test(answer)) {
        const match = regSelectData.exec(answer);

        if (!match) {
            console.log("Invalid select command format");
            return;
        }

        const tableName = match[1];
        let queryStr = match[2]; // 可能为 undefined

        let query = {};
        if (queryStr && typeof queryStr === 'string') { // 确保 queryStr 是字符串
            try {
                query = parseConditions(queryStr);
            } catch (error) {
                console.log(error.message);
                return;
            }
        }

        db.selectData(tableName, query);
    } else if (regSelectFields.test(answer)) {
        const match = regSelectFields.exec(answer);
        if (!match) {
            console.log("Invalid select command format");
            return;
        }
        const fieldsStr = match[1].trim();
        const tableName = match[2];
        let queryStr = match[3]; // 可能为 undefined

        let query = {};
        if (queryStr && typeof queryStr === 'string') {
            try {
                query = parseConditions(queryStr);
            } catch (error) {
                console.log(error.message);
                return;
            }
        }
        // 字段处理
        let fields = ['*'];
        if (fieldsStr !== '*') {
            fields = fieldsStr.split(',').map(f => f.trim()).filter(f => f.length > 0);
        }
        db.selectData(tableName, query, fields);
    } else if (reg16.test(answer)) {
        const extensionPath = reg16.exec(answer)[1];
        db.loadExtension(extensionPath);
    } else if (reg17.test(answer)) {
        const match = reg17.exec(answer);
        const extensionName = match[1].toLocaleLowerCase();
        if (!/^[a-zA-Z0-9_-]+$/.test(extensionName)) {
            console.log('无效扩展名格式，只能包含字母、数字、下划线和连字符');
            return;
        }
        let args = [];
        // 假设命令格式为 "extension 扩展名 参数1 参数2 ..."
        const argStr = answer.slice(answer.indexOf(extensionName) + extensionName.length).trim();
        if (argStr) {
            args = argStr.split(" ").filter(arg => arg.trim() !== ''); // 解析参数
        }
        db.executeExtension(extensionName, ...args);
    } else if (reg18.test(answer)) {
        const match = reg18.exec(answer);
        const tableName = match[1];
        const dimensionsStr = match[2];
        const dimensions = dimensionsStr.split(',');
        db.createCompositeIndex(tableName, dimensions);
    } else if (reg19.test(answer)) {
        db.showDatabases();
    } else if (reg20.test(answer)) {
        db.showTables();
    } else if (reg21.test(answer)) {
        const dbName = reg21.exec(answer)[1];
        db.dropdatabase(dbName);
        console.log(`Database "${dbName}" dropped.`);
    } else if (reg22.test(answer)) {
        const match = reg22.exec(answer);
        const tableName = match[1];
        const dimensions = match[2].split(',');

        // 生成可视化数据
        const points = db.projectToRiemannSphere(tableName, dimensions);
        //console.log("Visualization Data:");
        console.log(points.map(p =>
            `(${p.x.toFixed(2)}, ${p.y.toFixed(2)}, ${p.z.toFixed(2)})`
        ).join('\n'));

        // 生成Web可视化文件
        generateWebView(points);
    } else if (reg23.test(answer)) {
        const match = reg23.exec(answer);
        const tableName = match[1];
        const dimensionsStr = match[2];
        const funcStr = match[3];
        const sphereRadius = parseFloat(match[4]);
        const resolution = parseFloat(match[6]);

        // 将维度字符串转换为数组
        const dimensions = dimensionsStr.split(',');

        // 将函数字符串转换为函数对象
        const func = new Function(...dimensions, `return ${funcStr};`);

        // 插入交界点
        db.insertIntersectionPoints(tableName, dimensions, func, sphereRadius, resolution);
    } else if (reg24.test(answer)) {
        const match = reg24.exec(answer);
        const tableName = match[1];
        const dimensionsStr = match[2];
        const k = parseInt(match[3]);

        // 将维度字符串转换为数组
        const dimensions = dimensionsStr.split(',');

        // 执行K-均值聚类
        db.riemannKMeansClustering(tableName, dimensions, k);
    } else if (reg25.test(answer)) {
        const match = reg25.exec(answer);
        const tableName = match[1];
        const maxCapacity = parseInt(match[2]);
        const k = parseInt(match[3]);
        db.createPyramidIndex(tableName, maxCapacity, k);
    } else if (reg26.test(answer)) {
        const match = reg26.exec(answer);
        const tableName = match[1];
        const pointStr = match[2];
        const point = pointStr.split(',').map(Number);
        db.queryPyramidIndex(point);
    } else if (reg27.test(answer)) {
        const match = reg27.exec(answer);
        const tableName = match[1];
        db.createRecursiveSphereWeaving(tableName);
    } else if (reg28.test(answer)) {
        const match = reg28.exec(answer);
        const tableName = match[1];
        const pointStr = match[2];
        const point = pointStr.split(',').map(Number);
        db.queryRecursiveSphereWeaving(point);
    } else if (regQueryRSW.test(answer)) {
        const match = regQueryRSW.exec(answer);
        const tableName = match[1];
        const pointStr = match[2];
        const k = match[3] ? parseInt(match[3], 10) : 5;
        // 解析点坐标
        const point = pointStr.split(',').map(v => parseFloat(v.trim()));
        try {
            db.queryRecursiveSphereWeaving(tableName, point, k);
        } catch (error) {
            console.log(error.message);
        }
    } else if (reg29.test(answer)) { // 处理 execute script 命令
        const scriptPath = reg29.exec(answer)[1];
        executeScriptFile(scriptPath);
    } else if (regQueryCompositeIndex.test(answer)) {
        const match = regQueryCompositeIndex.exec(answer);
        const tableName = match[1];
        const dimsStr = match[2];
        let dimsStrFixed = dimsStr
            .replace(/([a-zA-Z0-9_]+)\s*:/g, '"$1":') // 键名加引号
            .replace(/'/g, '"'); // 单引号转双引号
        // 解析 {dimension1:[min,max],dimension2:[min,max],...}
        let dims = {};
        try {
            // 先把键名加引号，再整体转JSON
            dimsStrFixed = dimsStrFixed
                .replace(/([a-zA-Z0-9_]+)\s*:/g, '"$1":')
                .replace(/'/g, '"');
            dims = JSON.parse(`{${dimsStrFixed}}`);
        } catch (e) {
            console.log('维度参数解析失败:', e.message);
            return;
        }
        db.queryCompositeIndexRange(tableName, dims);
    } else // 新增压缩功能支持
        if (regEnableCompression.test(answer)) {
            const tableName = regEnableCompression.exec(answer)[1];
            if (db.data.has(db.currentDatabase) && db.data.get(db.currentDatabase).has(tableName)) {
                const table = db.data.get(db.currentDatabase).get(tableName);
                table.compressed = true; // 启用压缩
                console.log(`Compression enabled for table ${tableName}`);
            } else {
                console.log(`Table ${tableName} does not exist`);
            }
        } else if (regDisableCompression.test(answer)) {
            const tableName = regDisableCompression.exec(answer)[1];
            if (db.data.has(db.currentDatabase) && db.data.get(db.currentDatabase).has(tableName)) {
                const table = db.data.get(db.currentDatabase).get(tableName);
                table.compressed = false; // 禁用压缩
                console.log(`Compression disabled for table ${tableName}`);
            } else {
                console.log(`Table ${tableName} does not exist`);
            }
        } else if (regGeneratePoints.test(answer)) {
            const match = regGeneratePoints.exec(answer);
            const tableName = match[1];
            const distributionType = match[2];
            const parameters = match[3];

            // 解析参数
            const params = JSON.parse(`{${parameters}}`);

            // 生成数据点
            const dataPoints = generateDataPoints(distributionType, params);

            // 检查是否启用压缩
            if (db.data.has(db.currentDatabase) && db.data.get(db.currentDatabase).has(tableName)) {
                const table = db.data.get(db.currentDatabase).get(tableName);
                if (table.compressed) {
                    // 对生成的数据点进行压缩
                    const compressedPoints = dataPoints.map(point => db.conicalProjectionCompress(point));
                    db.insertBatch(tableName, compressedPoints);
                    console.log(`Generated and compressed ${dataPoints.length} points into table ${tableName}`);
                } else {
                    // 直接插入未压缩的数据点
                    db.insertBatch(tableName, dataPoints);
                    console.log(`Generated ${dataPoints.length} points into table ${tableName}`);
                }
            } else {
                console.log(`Table ${tableName} does not exist`);
            }
        } else if (regCloneTable.test(answer)) {
            const match = regCloneTable.exec(answer);
            const sourceTable = match[1];
            const targetTable = match[2];
            db.cloneTable(sourceTable, targetTable);
        } else if (regCloneDatabase.test(answer)) {
            const match = regCloneDatabase.exec(answer);
            const sourceDatabase = match[1];
            const targetDatabase = match[2];
            db.cloneDatabase(sourceDatabase, targetDatabase);
        } else if (regDestroyComposite.test(answer)) {
            const tableName = regDestroyComposite.exec(answer)[1];
            db.setnullcomposite(tableName);
        } else if (reg31.test(answer)) {
            db.setnullpyramid()
        } else if (reg32.test(answer)) {
            db.setnullrecursive()
        } else if (reg33.test(answer)) {
            const extensionName = reg33.exec(answer)[1];
            db.removeExtension(extensionName);
        } else if (reg34.test(answer)) {
            db.showExtensions();
        } else if (reg35.test(answer)) {
            // do nothing
        }
        else if (regMount.test(answer)) {
            const match = regMount.exec(answer);
            const dbName = match[1];
            const externalPath = match[2];
            db.mount(dbName, externalPath);
        } else if (regDemount.test(answer)) {
            const match = regDemount.exec(answer);
            const dbName = match[1];
            db.demount(dbName);
        } else if (regloadAll.test(answer)) {
            console.log("Loading all databases from disk...");
            db.loadDataAsync();
        } else {
            console.log("Invalid command, please use available commands or write 'exit' to escape");
        }
    displayPrompt();
}
function generateWebView(points) {
    const data = JSON.stringify(points);
    fs.writeFileSync('points.json', data);
    console.log('Visualization data saved: points.json');
}
// 开始命令行交互
rl.on('line', (answer) => {
    executePrompt(answer);


});

// 修改 rl.on('close', ...) 事件监听器
rl.on('close', () => {
    if (!this.isDroppingDatabase) { // 如果不是正在删除数据库，则保存数据
        db.saveData();
    }
    console.log('thank you for using Devilfish DBMS system, see you next time!');
    process.exit(0);
});

module.exports = executePrompt();