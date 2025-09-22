/*# Devilfish DBMS 用户手册 & API 使用说明

Devilfish DBMS 是一款高性能的数据库管理系统，专为高维数据处理、分析与可视化设计，支持丰富的 SQL 扩展、机器学习、张量操作、数据导入导出等功能。支持 HTTP API 及命令行两种操作方式。

# 启动方式

```bash
# Download and install nvm:
curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.40.3/install.sh | bash
# in lieu of restarting the shell
\. "$HOME/.nvm/nvm.sh"
# Download and install Node.js:
nvm install 22
# Verify the Node.js version:
node -v # Should print "v22.17.1".
nvm current # Should print "v22.17.1".
# Verify npm version:
npm -v # Should print "10.9.2".
npm install -g cnpm --registry=https://registry.npmmirror.com
git clone https://gitee.com/mumu2009/devilfish-dbms.git
cd devilfish-dbms
cnpm install
node api.js
```

#### 注意，high-vx.x-cluster的启动方式有部分不同，详见./high-v3.0-cluster/readme.md

---

## 一、系统特性

- 高维数据存储与查询
- 分区表支持（基于哈希分区）
- 多种索引与聚类算法（复合索引、金字塔索引、递归球面编织、KMeans等）
- SQL 扩展与批量操作
- 机器学习模型（线性回归、朴素贝叶斯、多项式拟合等）
- 张量创建与运算
- 数据可视化与 Web 可视化文件生成
- 数据压缩与解压缩（黎曼球面映射压缩）
- 扩展模块与脚本支持
- 数据导入导出能力（CSV等）
- 模糊函数与泰勒展开
- 交集点插入与几何运算
  
  ##### 在本数据库中 每个表最大只能一次性初始化2^29个字符，也就是说 所有的sql语句的长度不能超过2^29=536870912=524.288MB个字符，在执行脚本时为整个脚本的长度不应超过这一限制

---

## 二、API 启动与基础信息

- 默认监听端口：`3125`
- 支持外网访问（监听 `0.0.0.0`）
- 支持 CORS 跨域
- 支持 JSON 及 URL 编码表单请求

### 1. `/execute` 路由

- **方法**：POST
- **参数**：JSON 格式，需包含 `command` 字段
- **返回**：执行日志文本

#### 示例请求

```json
POST /execute
{
  "command": "create database test"
}
```

#### 返回示例

```
Database created
Command executed successfully
```

---

## 三、命令总览

#### 注意，下文中的指令除单列一行进行说明外，功能一般同mysql的同名命令。标有'*'的功能表示下个版本开放，标有'**'的功能则会在之后的版本中开放（不包含下一个版本）所有的功能后均没有分号，以换行为下一条指令的标志

### 1. 数据库管理

- `create database <database_name>`
- `use <database_name>`
- `drop database <database_name>`
- `show databases`
- `clone database <source_database> to <target_database>`
- `mount database from ./path`
- `demount database`

### 2. 表管理

- `create table <table_name> (<column1>,<column2>, ...) partition by <partition_key> partitions = <num_partitions>`
  
  注意，这里'()'内不能有空格，支持指定分区数量，默认有10个基于hash(partition_key)的分区
  同时，由于硬盘存储的限制，目前必须有id字段（如果为硬盘表），同时id项不能为0，建议id项放在末尾
  如果想要更改重新加载后的必须有的字段，可以搜索并修改loaddataasync和_loadsignalDatabase方法中的partitionKey的值

- `add dimension <column_name> to <table_name>`
  
  这里表示在一个表上添加一个维度（字段）

- `remove dimension <column_name> from <table_name>`
  
  这里表示在一个表上删除一个维度（字段）

- `drop table <table_name>`

- `show tables`

- `clone table <source_table> to <target_table>`

### 3. 数据操作

#### 基础操作

- `insert into <table_name> values {<key1>:<value1>, ...}`

- `insert batch into <table_name> values [{...}, {...}]`
  
  批量插入，每一个{}中的内容同上一个insert into 指令中的values值，在数据量较大时有优势，数据量小时有劣势
  
  每一次插入的最大值最好不要超过72MB 否则无法保证数据库的稳定性

#### 更新操作

- `update <table_name> set <column>=<new_value> where <condition>`
  
  注意condition不能省略

- `update batch <table_name> set [{key:<primary_key_value>, newData:{<field1>:<value1>, ...}}, ...]`
  
  批量更新多行记录

- `update batch column <table_name> set {<field1>=<value1>, <field2>=<value2>} where <condition>`
  
  批量更新指定条件下的列

#### 删除操作

- `delete from <table_name> where <condition>`
  
  注意condition不能省略，支持主键精确匹配O(1)操作

#### 查询操作

- `select * from <table_name> where <condition>`
- `select <column1>,<column2>,... from <table_name> where <condition>`

#### 条件语法支持

支持复杂的WHERE条件：

- 基础比较：`=`, `>`, `<`, `>=`, `<=`, `<>`
- 逻辑运算：`AND`, `OR`, `NOT`
- 子查询：`{column:target_column, subquery:SELECT statement}`

### 4. 索引与高维空间

**注意：所有的索引在同一时间只能有一个，所有的索引都可以动态增/删/改，但是速度较慢，建议在多次插入时直接删除索引并重建**

#### 复合索引

- `create composite index on <table_name> dimensions <dimension1>,<dimension2>,...`
  
  复合索引，基于表上的维度

- `query composite index on <table_name> with dimensions {dim1:[min,max], dim2:[min,max], ...}`
  
  根据索引composite index，查找dim范围内的信息

- `destroy composite index on <table_name>`

#### 金字塔索引

- `create pyramid index on <table_name> with max_capacity=<max_capacity> and k=<k>`
  
  金字塔索引(树状索引)，基于表，max_capacity决定了叶层中每个叶上的点个数，建议max_capacity>k

- `query pyramid index on <table_name> with point=[x,y,...]`
  
  根据索引pyramid index查找距离某一点最近的k个点

- `destroy pyramid index`

#### 递归球面编织索引

- `create recursive sphere weaving on <table_name>`
  
  递归球面编织为集合索引，将高维点折射到低维

- `query recursive sphere weaving on <table_name> with point=[x,y,...] with k=<k>`

- `query recursive sphere weaving on <table_name> with point=[x,y,...]`
  
  根据索引recursive sphere weaving查找距离某一点最近的k个点（默认5个）

- `destroy recursive sphere weaving`

### 5. 数据可视化

- `visualize <table_name> with <dimensions>`
  
  加载完成后，可以在API的'/'下查看(get)，该选项dimensions应为三个分量，但注意，只有前两个分量有用，根据这两个分量数据点将被映射到球面上

### 6. 脚本与扩展

#### 扩展管理

- `load extension <extension_path>`
- `extension <extension_name> <command> [args...]`
- `remove extension <extension_name>`
- `show extensions`

#### 脚本执行

- `execute script <script_path>`
  
  这里的script为一个命名为*.sql的文件，文件中是由sql语句组成的组合
  
  例：
  
  ```sql
  sql1
  sql2
  sql3
  ```

### 7. 数据生成与噪声

#### 注意：这些功能需要先加载 ai-direction 扩展模块：

```
load extension main-ai-direction.js
```

#### 数据点生成

- `generate points on <table_name> with distribution <distribution_type> and parameters {<param1>:<value1>, ...}`
  
  支持的分布类型：
  
  - uniform: 均匀分布，参数格式：`{numPoints:100, x:{min:0,max:10}, y:{min:0,max:10}}`
  - normal: 正态分布，参数格式：`{numPoints:100, x:{mean:0,stdDev:1}, y:{mean:0,stdDev:1}}`
  - sphere: 球面分布，参数格式：`{numPoints:100, radius:1}`

- `extension ai-direction generate <table_name> <distribution_type> <count> <param1> <param2>`
  
  生成指定分布的数据点并插入表中
  
  - distribution_type: normal/gaussian（正态分布）或 uniform（均匀分布）
  - count: 生成的点数
  - 正态分布参数：mean（均值）, stdDev（标准差）
  - 均匀分布参数：min（最小值）, max（最大值）

- `extension ai-direction addnoise <table_name> <noise_type> <column> <param1> <param2>`
  
  为表中指定列添加噪声
  
  - noise_type: gaussian（高斯噪声）
  - column: 目标列名
  - 高斯噪声参数：mean（均值）, stdDev（标准差）

### 8. 数据压缩与解压缩

#### 压缩对性能损失较大，最好不要启用压缩功能，默认该选项为disable

- `enable compression for <table_name>`
- `disable compression for <table_name>`

**系统采用黎曼球面映射压缩算法，支持有损压缩，适用于高维数据降维存储**

### 9. 数据导入与导出

- `import csv <table_name> from <file_path>`
  
  从CSV文件导入数据，支持自动类型推断和数据清理

### 10. 几何运算与数学分析

#### 模糊函数与泰勒展开

- `get fuzzy function <table_name> on <dimension1> and <dimension2>`
  
  获取基于两个维度的模糊函数

- `taylor expand <table_name> on <dimension1> and <dimension2> at (<x0>,<y0>) order <order>`
  
  在指定点进行泰勒展开

#### 交集点插入

- `insert intersection into <table_name> dimensions <dimension1>,<dimension2>,... function <function_expression> radius <radius> resolution <resolution>`
  
  计算函数与球面的交集点并插入表中

### 11. 聚类分析

- `kmeans cluster <table_name> on <dimension1>,<dimension2>,... with k <k>`
  
  对指定维度执行K-means聚类，支持多维数据聚类，结果将添加cluster_label字段

### 12. SQL 扩展与统计分析

#### 注意：这些功能需要先加载 sql-direction 扩展模块：

```
load extension main-sql-direction.js
```

#### 逻辑运算

- `extension sql-direction or <table_name> <query1> <query2>`
  
  对两个查询结果执行 OR 操作，返回并集

- `extension sql-direction and <table_name> <query1> <query2>`
  
  对两个查询结果执行 AND 操作，返回交集

#### 表连接

- `extension sql-direction join <table_name1> <table_name2> <join_key>`
  
  基于指定键对两个表执行内连接

#### 排序与限制

- `extension sql-direction order <table_name> <column> [asc|desc]`
  
  按指定列对结果进行升序或降序排序（默认升序）

- `extension sql-direction limit <table_name> <limit_number>`
  
  限制返回结果的数量

#### 数据选择与复制

- `extension sql-direction select <table_name> <new_table_name> <query>`
  
  将查询结果插入到新表中

#### 统计函数

- `extension sql-direction average <table_name> <column>`
  
  计算指定列的平均值

- `extension sql-direction amount <table_name>`
  
  返回表中记录的总数

- `extension sql-direction variance <table_name> <column>`
  
  计算指定列的方差

- `extension sql-direction min <table_name> <column>`
  
  返回指定列的最小值

- `extension sql-direction max <table_name> <column>`
  
  返回指定列的最大值

- `extension sql-direction sum <table_name> <column>`
  
  计算指定列的总和

- `extension sql-direction median <table_name> <column>`
  
  计算指定列的中位数

#### 分组与聚合

- `extension sql-direction group <table_name> <group_column> <aggregate_column> <aggregate_function>`
  
  按指定列分组并应用聚合函数（sum, avg, min, max, count）

- `extension sql-direction having <table_name> <group_column> <aggregate_column> <aggregate_function> <condition>`
  
  对分组结果应用 HAVING 条件过滤

#### 子查询与存在性检查

- `extension sql-direction exists <table_name> <sub_query>`
  
  检查子查询是否返回结果

#### 数据格式化

- `extension sql-direction round <table_name> <column> <precision>`
  
  将指定列的数值四舍五入到指定精度

- `extension sql-direction format <table_name> <column> <format_string>`
  
  使用格式字符串格式化指定列（支持 {} 和 {value} 占位符）

### 13. 机器学习扩展

#### 注意：这些功能需要先加载 ai-direction 扩展模块：

```
load extension main-ai-direction.js
```

#### 线性回归

- `extension ai-direction createlinear <table_name> <features> <target>`
  
  创建线性回归模型
  
  - features: 特征列名，多个用逗号分隔
  - target: 目标列名

- `extension ai-direction predictlinear <table_name> <value1> <value2> ...`
  
  使用线性回归模型进行预测

#### 朴素贝叶斯

- `extension ai-direction createbayes <table_name> <features> <target>`
  
  创建朴素贝叶斯模型
  
  - features: 特征列名，多个用逗号分隔
  - target: 目标列名

- `extension ai-direction predictbayes <table_name> <value1> <value2> ...`
  
  使用朴素贝叶斯模型进行预测

#### 多项式拟合

- `extension ai-direction polynomial <table_name> <degree> [x_column] [y_column]`
  
  对数据进行多项式拟合
  
  - degree: 多项式阶数
  - x_column: X轴列名（默认为'x'）
  - y_column: Y轴列名（默认为'y'）

#### 张量操作

- `extension ai-direction createtensor <tensor_name> <rows> <cols>`
  
  创建指定形状的张量（当前仅支持2D张量）

- `extension ai-direction tensor <operation> <tensor1_name> [tensor2_name]`
  
  执行张量运算
  
  - operation: transpose（转置）, add（加法）, multiply（矩阵乘法）
  - 转置操作只需一个张量，其他操作需要两个张量

### 14. 其它功能

- `exit`
- `-- <comment>`（注释行，以--开头的行会被忽略）
- `load all`重新从硬盘中加载数据

---

## 四、高级特性说明

### 1. 分区表架构

- 采用哈希分区策略，根据分区键自动分配数据到不同分区
- 支持自定义分区数量，默认10个分区
- 主键查询支持O(1)直接定位分区
- 分区间负载自动均衡

### 2. 压缩算法

- 采用黎曼球面映射压缩算法
- 支持高维数据到低维的有损压缩
- 圆锥投影技术实现空间数据压缩
- 可配置压缩参数和焦点

### 3. 索引优化

- 复合索引支持多维范围查询
- 金字塔索引提供k近邻搜索
- 递归球面编织实现高维数据降维索引
- 索引动态维护，支持实时更新

### 4. 查询优化

- 主键查询自动优化为O(1)操作
- 复杂条件查询支持索引加速
- 查询结果缓存机制
- 分区剪枝优化

---

## 五、日志与调试

- 所有命令执行日志会通过 `console.log` 输出，并写入 `dbms.log` 文件。
- `/execute` 路由返回本次命令的所有日志输出。
- 所有 `console.log(obj)` 会自动转为 JSON 字符串，便于日志分析。
- 支持DEBUG、INFO、WARN、ERROR四个日志级别

---

## 六、批量导入与性能建议

- 大批量插入时建议关闭自动持久化，插入完毕后再手动保存数据库。
- 大批量导入时建议删除所有索引，导入完成后再重建索引。
- `insert batch` 指令不自动保存，建议插入或更改一条工具表数据以触发保存。
- 使用批量更新命令提高大量数据修改的性能
- 合理设置分区数量以优化查询性能

---

## 七、Web 可视化

- 访问 `http://<服务器IP>:3125/entrence` 可查看 `entr.html` 页面（需自行开发前端）。
- 支持生成points.json可视化数据文件
- 支持三维球面投影可视化

---

## 八、扩展与自定义

- 支持通过 `load extension 扩展路径` 加载自定义扩展
- 扩展需导出包含execute对象的模块
- 支持扩展的动态加载、卸载和管理
- 内置ai-direction和sql-direction扩展模块

---

## 九、常见问题

- 数据库文件损坏修复：如遇 `incorrect header check`，请执行 `node repairdb.js` 查看说明。
- 速度过低：注意剩余内存，本数据库的性能极大地取决于内存量
- 分区键选择：建议选择数据分布均匀的字段作为分区键
- 索引选择：根据查询模式选择合适的索引类型
- 压缩使用：仅在存储空间紧张时启用压缩功能

---

## 十、示例：通过 HTTP 执行命令

```bash
# 创建数据库
curl -X POST http://localhost:3125/execute -H "Content-Type: application/json" -d "{\"command\":\"create database test\"}"

# 使用数据库
curl -X POST http://localhost:3125/execute -H "Content-Type: application/json" -d "{\"command\":\"use test\"}"

# 创建表
curl -X POST http://localhost:3125/execute -H "Content-Type: application/json" -d "{\"command\":\"create table users (id,name,age) partition by id partitions = 5\"}"

# 插入数据
curl -X POST http://localhost:3125/execute -H "Content-Type: application/json" -d "{\"command\":\"insert into users values {id:1, name:Alice, age:25}\"}"

# 查询数据
curl -X POST http://localhost:3125/execute -H "Content-Type: application/json" -d "{\"command\":\"select * from users where id=1\"}"
```

---

## 十一、完整示例工作流

```sql
-- 创建和使用数据库
create database analytics
use analytics

-- 创建分区表
create table sales_data (id,region,amount,date) partition by region partitions = 8

-- 批量插入数据
insert batch into sales_data values [
  {id:1, region:north, amount:1000, date:2024-01-01},
  {id:2, region:south, amount:1500, date:2024-01-02},
  {id:3, region:east, amount:1200, date:2024-01-03}
]

-- 创建复合索引
create composite index on sales_data dimensions region,amount

-- 范围查询
query composite index on sales_data with dimensions {region:[north,south], amount:[1000,2000]}

-- K-means聚类
kmeans cluster sales_data on amount,id with k 3

-- 数据可视化
visualize sales_data with amount,id,region

-- 加载扩展并执行统计
load extension main-sql-direction.js
extension sql-direction average sales_data amount
extension sql-direction group sales_data region amount sum
```

---

如需更多命令或高级用法，请查阅源码或联系开发。
*/
var __getOwnPropNames = Object.getOwnPropertyNames;
var __commonJS = (cb, mod) => function __require() {
  return mod || (0, cb[__getOwnPropNames(cb)[0]])((mod = { exports: {} }).exports, mod), mod.exports;
};

// node_modules/.store/csv-parser@3.2.0/node_modules/csv-parser/index.js
var require_csv_parser = __commonJS({
  "node_modules/.store/csv-parser@3.2.0/node_modules/csv-parser/index.js"(exports2, module2) {
    var { Transform } = require("stream");
    var [cr] = Buffer.from("\r");
    var [nl] = Buffer.from("\n");
    var defaults = {
      escape: '"',
      headers: null,
      mapHeaders: ({ header }) => header,
      mapValues: ({ value }) => value,
      newline: "\n",
      quote: '"',
      raw: false,
      separator: ",",
      skipComments: false,
      skipLines: null,
      maxRowBytes: Number.MAX_SAFE_INTEGER,
      strict: false,
      outputByteOffset: false
    };
    var CsvParser = class extends Transform {
      constructor(opts = {}) {
        super({ objectMode: true, highWaterMark: 16 });
        if (Array.isArray(opts)) opts = { headers: opts };
        const options = Object.assign({}, defaults, opts);
        options.customNewline = options.newline !== defaults.newline;
        for (const key of ["newline", "quote", "separator"]) {
          if (typeof options[key] !== "undefined") {
            [options[key]] = Buffer.from(options[key]);
          }
        }
        options.escape = (opts || {}).escape ? Buffer.from(options.escape)[0] : options.quote;
        this.state = {
          empty: options.raw ? Buffer.alloc(0) : "",
          escaped: false,
          first: true,
          lineNumber: 0,
          previousEnd: 0,
          rowLength: 0,
          quoted: false
        };
        this._prev = null;
        if (options.headers === false) {
          options.strict = false;
        }
        if (options.headers || options.headers === false) {
          this.state.first = false;
        }
        this.options = options;
        this.headers = options.headers;
        this.bytesRead = 0;
      }
      parseCell(buffer, start, end) {
        const { escape, quote } = this.options;
        if (buffer[start] === quote && buffer[end - 1] === quote) {
          start++;
          end--;
        }
        let y = start;
        for (let i = start; i < end; i++) {
          if (buffer[i] === escape && i + 1 < end && buffer[i + 1] === quote) {
            i++;
          }
          if (y !== i) {
            buffer[y] = buffer[i];
          }
          y++;
        }
        return this.parseValue(buffer, start, y);
      }
      parseLine(buffer, start, end) {
        const { customNewline, escape, mapHeaders, mapValues, quote, separator, skipComments, skipLines } = this.options;
        end--;
        if (!customNewline && buffer.length && buffer[end - 1] === cr) {
          end--;
        }
        const comma = separator;
        const cells = [];
        let isQuoted = false;
        let offset = start;
        if (skipComments) {
          const char = typeof skipComments === "string" ? skipComments : "#";
          if (buffer[start] === Buffer.from(char)[0]) {
            return;
          }
        }
        const mapValue = (value) => {
          if (this.state.first) {
            return value;
          }
          const index = cells.length;
          const header = this.headers[index];
          return mapValues({ header, index, value });
        };
        for (let i = start; i < end; i++) {
          const isStartingQuote = !isQuoted && buffer[i] === quote;
          const isEndingQuote = isQuoted && buffer[i] === quote && i + 1 <= end && buffer[i + 1] === comma;
          const isEscape = isQuoted && buffer[i] === escape && i + 1 < end && buffer[i + 1] === quote;
          if (isStartingQuote || isEndingQuote) {
            isQuoted = !isQuoted;
            continue;
          } else if (isEscape) {
            i++;
            continue;
          }
          if (buffer[i] === comma && !isQuoted) {
            let value = this.parseCell(buffer, offset, i);
            value = mapValue(value);
            cells.push(value);
            offset = i + 1;
          }
        }
        if (offset < end) {
          let value = this.parseCell(buffer, offset, end);
          value = mapValue(value);
          cells.push(value);
        }
        if (buffer[end - 1] === comma) {
          cells.push(mapValue(this.state.empty));
        }
        const skip = skipLines && skipLines > this.state.lineNumber;
        this.state.lineNumber++;
        if (this.state.first && !skip) {
          this.state.first = false;
          this.headers = cells.map((header, index) => mapHeaders({ header, index }));
          this.emit("headers", this.headers);
          return;
        }
        if (!skip && this.options.strict && cells.length !== this.headers.length) {
          const e = new RangeError("Row length does not match headers");
          this.emit("error", e);
        } else {
          if (!skip) {
            const byteOffset = this.bytesRead - buffer.length + start;
            this.writeRow(cells, byteOffset);
          }
        }
      }
      parseValue(buffer, start, end) {
        if (this.options.raw) {
          return buffer.slice(start, end);
        }
        return buffer.toString("utf-8", start, end);
      }
      writeRow(cells, byteOffset) {
        const headers = this.headers === false ? cells.map((value, index) => index) : this.headers;
        const row = cells.reduce((o, cell, index) => {
          const header = headers[index];
          if (header === null) return o;
          if (header !== void 0) {
            o[header] = cell;
          } else {
            o[`_${index}`] = cell;
          }
          return o;
        }, {});
        if (this.options.outputByteOffset) {
          this.push({ row, byteOffset });
        } else {
          this.push(row);
        }
      }
      _flush(cb) {
        if (this.state.escaped || !this._prev) return cb();
        this.parseLine(this._prev, this.state.previousEnd, this._prev.length + 1);
        cb();
      }
      _transform(data, enc, cb) {
        if (typeof data === "string") {
          data = Buffer.from(data);
        }
        const { escape, quote } = this.options;
        let start = 0;
        let buffer = data;
        this.bytesRead += data.byteLength;
        if (this._prev) {
          start = this._prev.length;
          buffer = Buffer.concat([this._prev, data]);
          this._prev = null;
        }
        const bufferLength = buffer.length;
        for (let i = start; i < bufferLength; i++) {
          const chr = buffer[i];
          const nextChr = i + 1 < bufferLength ? buffer[i + 1] : null;
          this.state.rowLength++;
          if (this.state.rowLength > this.options.maxRowBytes) {
            return cb(new Error("Row exceeds the maximum size"));
          }
          if (!this.state.escaped && chr === escape && nextChr === quote && i !== start) {
            this.state.escaped = true;
            continue;
          } else if (chr === quote) {
            if (this.state.escaped) {
              this.state.escaped = false;
            } else {
              this.state.quoted = !this.state.quoted;
            }
            continue;
          }
          if (!this.state.quoted) {
            if (this.state.first && !this.options.customNewline) {
              if (chr === nl) {
                this.options.newline = nl;
              } else if (chr === cr) {
                if (nextChr !== nl) {
                  this.options.newline = cr;
                }
              }
            }
            if (chr === this.options.newline) {
              this.parseLine(buffer, this.state.previousEnd, i + 1);
              this.state.previousEnd = i + 1;
              this.state.rowLength = 0;
            }
          }
        }
        if (this.state.previousEnd === bufferLength) {
          this.state.previousEnd = 0;
          return cb();
        }
        if (bufferLength - this.state.previousEnd < data.length) {
          this._prev = data;
          this.state.previousEnd -= bufferLength - data.length;
          return cb();
        }
        this._prev = buffer;
        cb();
      }
    };
    module2.exports = (opts) => new CsvParser(opts);
  }
});

// ../node_modules/.store/better-sqlite3@12.2.0/node_modules/better-sqlite3/lib/util.js
var require_util = __commonJS({
  "../node_modules/.store/better-sqlite3@12.2.0/node_modules/better-sqlite3/lib/util.js"(exports2) {
    "use strict";
    exports2.getBooleanOption = (options, key) => {
      let value = false;
      if (key in options && typeof (value = options[key]) !== "boolean") {
        throw new TypeError(`Expected the "${key}" option to be a boolean`);
      }
      return value;
    };
    exports2.cppdb = Symbol();
    exports2.inspect = Symbol.for("nodejs.util.inspect.custom");
  }
});

// ../node_modules/.store/better-sqlite3@12.2.0/node_modules/better-sqlite3/lib/sqlite-error.js
var require_sqlite_error = __commonJS({
  "../node_modules/.store/better-sqlite3@12.2.0/node_modules/better-sqlite3/lib/sqlite-error.js"(exports2, module2) {
    "use strict";
    var descriptor = { value: "SqliteError", writable: true, enumerable: false, configurable: true };
    function SqliteError(message, code) {
      if (new.target !== SqliteError) {
        return new SqliteError(message, code);
      }
      if (typeof code !== "string") {
        throw new TypeError("Expected second argument to be a string");
      }
      Error.call(this, message);
      descriptor.value = "" + message;
      Object.defineProperty(this, "message", descriptor);
      Error.captureStackTrace(this, SqliteError);
      this.code = code;
    }
    Object.setPrototypeOf(SqliteError, Error);
    Object.setPrototypeOf(SqliteError.prototype, Error.prototype);
    Object.defineProperty(SqliteError.prototype, "name", descriptor);
    module2.exports = SqliteError;
  }
});

// ../node_modules/.store/file-uri-to-path@1.0.0/node_modules/file-uri-to-path/index.js
var require_file_uri_to_path = __commonJS({
  "../node_modules/.store/file-uri-to-path@1.0.0/node_modules/file-uri-to-path/index.js"(exports2, module2) {
    var sep = require("path").sep || "/";
    module2.exports = fileUriToPath;
    function fileUriToPath(uri) {
      if ("string" != typeof uri || uri.length <= 7 || "file://" != uri.substring(0, 7)) {
        throw new TypeError("must pass in a file:// URI to convert to a file path");
      }
      var rest = decodeURI(uri.substring(7));
      var firstSlash = rest.indexOf("/");
      var host = rest.substring(0, firstSlash);
      var path2 = rest.substring(firstSlash + 1);
      if ("localhost" == host) host = "";
      if (host) {
        host = sep + sep + host;
      }
      path2 = path2.replace(/^(.+)\|/, "$1:");
      if (sep == "\\") {
        path2 = path2.replace(/\//g, "\\");
      }
      if (/^.+\:/.test(path2)) {
      } else {
        path2 = sep + path2;
      }
      return host + path2;
    }
  }
});

// ../node_modules/.store/bindings@1.5.0/node_modules/bindings/bindings.js
var require_bindings = __commonJS({
  "../node_modules/.store/bindings@1.5.0/node_modules/bindings/bindings.js"(exports2, module2) {
    var fs2 = require("fs");
    var path2 = require("path");
    var fileURLToPath = require_file_uri_to_path();
    var join = path2.join;
    var dirname = path2.dirname;
    var exists = fs2.accessSync && function(path3) {
      try {
        fs2.accessSync(path3);
      } catch (e) {
        return false;
      }
      return true;
    } || fs2.existsSync || path2.existsSync;
    var defaults = {
      arrow: process.env.NODE_BINDINGS_ARROW || " \u2192 ",
      compiled: process.env.NODE_BINDINGS_COMPILED_DIR || "compiled",
      platform: process.platform,
      arch: process.arch,
      nodePreGyp: "node-v" + process.versions.modules + "-" + process.platform + "-" + process.arch,
      version: process.versions.node,
      bindings: "bindings.node",
      try: [
        // node-gyp's linked version in the "build" dir
        ["module_root", "build", "bindings"],
        // node-waf and gyp_addon (a.k.a node-gyp)
        ["module_root", "build", "Debug", "bindings"],
        ["module_root", "build", "Release", "bindings"],
        // Debug files, for development (legacy behavior, remove for node v0.9)
        ["module_root", "out", "Debug", "bindings"],
        ["module_root", "Debug", "bindings"],
        // Release files, but manually compiled (legacy behavior, remove for node v0.9)
        ["module_root", "out", "Release", "bindings"],
        ["module_root", "Release", "bindings"],
        // Legacy from node-waf, node <= 0.4.x
        ["module_root", "build", "default", "bindings"],
        // Production "Release" buildtype binary (meh...)
        ["module_root", "compiled", "version", "platform", "arch", "bindings"],
        // node-qbs builds
        ["module_root", "addon-build", "release", "install-root", "bindings"],
        ["module_root", "addon-build", "debug", "install-root", "bindings"],
        ["module_root", "addon-build", "default", "install-root", "bindings"],
        // node-pre-gyp path ./lib/binding/{node_abi}-{platform}-{arch}
        ["module_root", "lib", "binding", "nodePreGyp", "bindings"]
      ]
    };
    function bindings(opts) {
      if (typeof opts == "string") {
        opts = { bindings: opts };
      } else if (!opts) {
        opts = {};
      }
      Object.keys(defaults).map(function(i2) {
        if (!(i2 in opts)) opts[i2] = defaults[i2];
      });
      if (!opts.module_root) {
        opts.module_root = exports2.getRoot(exports2.getFileName());
      }
      if (path2.extname(opts.bindings) != ".node") {
        opts.bindings += ".node";
      }
      var requireFunc = typeof __webpack_require__ === "function" ? __non_webpack_require__ : require;
      var tries = [], i = 0, l = opts.try.length, n, b, err;
      for (; i < l; i++) {
        n = join.apply(
          null,
          opts.try[i].map(function(p) {
            return opts[p] || p;
          })
        );
        tries.push(n);
        try {
          b = opts.path ? requireFunc.resolve(n) : requireFunc(n);
          if (!opts.path) {
            b.path = n;
          }
          return b;
        } catch (e) {
          if (e.code !== "MODULE_NOT_FOUND" && e.code !== "QUALIFIED_PATH_RESOLUTION_FAILED" && !/not find/i.test(e.message)) {
            throw e;
          }
        }
      }
      err = new Error(
        "Could not locate the bindings file. Tried:\n" + tries.map(function(a) {
          return opts.arrow + a;
        }).join("\n")
      );
      err.tries = tries;
      throw err;
    }
    module2.exports = exports2 = bindings;
    exports2.getFileName = function getFileName(calling_file) {
      var origPST = Error.prepareStackTrace, origSTL = Error.stackTraceLimit, dummy = {}, fileName;
      Error.stackTraceLimit = 10;
      Error.prepareStackTrace = function(e, st) {
        for (var i = 0, l = st.length; i < l; i++) {
          fileName = st[i].getFileName();
          if (fileName !== __filename) {
            if (calling_file) {
              if (fileName !== calling_file) {
                return;
              }
            } else {
              return;
            }
          }
        }
      };
      Error.captureStackTrace(dummy);
      dummy.stack;
      Error.prepareStackTrace = origPST;
      Error.stackTraceLimit = origSTL;
      var fileSchema = "file://";
      if (fileName.indexOf(fileSchema) === 0) {
        fileName = fileURLToPath(fileName);
      }
      return fileName;
    };
    exports2.getRoot = function getRoot(file) {
      var dir = dirname(file), prev;
      while (true) {
        if (dir === ".") {
          dir = process.cwd();
        }
        if (exists(join(dir, "package.json")) || exists(join(dir, "node_modules"))) {
          return dir;
        }
        if (prev === dir) {
          throw new Error(
            'Could not find module root given file: "' + file + '". Do you have a `package.json` file? '
          );
        }
        prev = dir;
        dir = join(dir, "..");
      }
    };
  }
});

// ../node_modules/.store/better-sqlite3@12.2.0/node_modules/better-sqlite3/lib/methods/wrappers.js
var require_wrappers = __commonJS({
  "../node_modules/.store/better-sqlite3@12.2.0/node_modules/better-sqlite3/lib/methods/wrappers.js"(exports2) {
    "use strict";
    var { cppdb } = require_util();
    exports2.prepare = function prepare(sql) {
      return this[cppdb].prepare(sql, this, false);
    };
    exports2.exec = function exec(sql) {
      this[cppdb].exec(sql);
      return this;
    };
    exports2.close = function close() {
      this[cppdb].close();
      return this;
    };
    exports2.loadExtension = function loadExtension(...args) {
      this[cppdb].loadExtension(...args);
      return this;
    };
    exports2.defaultSafeIntegers = function defaultSafeIntegers(...args) {
      this[cppdb].defaultSafeIntegers(...args);
      return this;
    };
    exports2.unsafeMode = function unsafeMode(...args) {
      this[cppdb].unsafeMode(...args);
      return this;
    };
    exports2.getters = {
      name: {
        get: function name() {
          return this[cppdb].name;
        },
        enumerable: true
      },
      open: {
        get: function open() {
          return this[cppdb].open;
        },
        enumerable: true
      },
      inTransaction: {
        get: function inTransaction() {
          return this[cppdb].inTransaction;
        },
        enumerable: true
      },
      readonly: {
        get: function readonly() {
          return this[cppdb].readonly;
        },
        enumerable: true
      },
      memory: {
        get: function memory() {
          return this[cppdb].memory;
        },
        enumerable: true
      }
    };
  }
});

// ../node_modules/.store/better-sqlite3@12.2.0/node_modules/better-sqlite3/lib/methods/transaction.js
var require_transaction = __commonJS({
  "../node_modules/.store/better-sqlite3@12.2.0/node_modules/better-sqlite3/lib/methods/transaction.js"(exports2, module2) {
    "use strict";
    var { cppdb } = require_util();
    var controllers = /* @__PURE__ */ new WeakMap();
    module2.exports = function transaction(fn) {
      if (typeof fn !== "function") throw new TypeError("Expected first argument to be a function");
      const db2 = this[cppdb];
      const controller = getController(db2, this);
      const { apply } = Function.prototype;
      const properties = {
        default: { value: wrapTransaction(apply, fn, db2, controller.default) },
        deferred: { value: wrapTransaction(apply, fn, db2, controller.deferred) },
        immediate: { value: wrapTransaction(apply, fn, db2, controller.immediate) },
        exclusive: { value: wrapTransaction(apply, fn, db2, controller.exclusive) },
        database: { value: this, enumerable: true }
      };
      Object.defineProperties(properties.default.value, properties);
      Object.defineProperties(properties.deferred.value, properties);
      Object.defineProperties(properties.immediate.value, properties);
      Object.defineProperties(properties.exclusive.value, properties);
      return properties.default.value;
    };
    var getController = (db2, self) => {
      let controller = controllers.get(db2);
      if (!controller) {
        const shared = {
          commit: db2.prepare("COMMIT", self, false),
          rollback: db2.prepare("ROLLBACK", self, false),
          savepoint: db2.prepare("SAVEPOINT `	_bs3.	`", self, false),
          release: db2.prepare("RELEASE `	_bs3.	`", self, false),
          rollbackTo: db2.prepare("ROLLBACK TO `	_bs3.	`", self, false)
        };
        controllers.set(db2, controller = {
          default: Object.assign({ begin: db2.prepare("BEGIN", self, false) }, shared),
          deferred: Object.assign({ begin: db2.prepare("BEGIN DEFERRED", self, false) }, shared),
          immediate: Object.assign({ begin: db2.prepare("BEGIN IMMEDIATE", self, false) }, shared),
          exclusive: Object.assign({ begin: db2.prepare("BEGIN EXCLUSIVE", self, false) }, shared)
        });
      }
      return controller;
    };
    var wrapTransaction = (apply, fn, db2, { begin, commit, rollback, savepoint, release, rollbackTo }) => function sqliteTransaction() {
      let before, after, undo;
      if (db2.inTransaction) {
        before = savepoint;
        after = release;
        undo = rollbackTo;
      } else {
        before = begin;
        after = commit;
        undo = rollback;
      }
      before.run();
      try {
        const result = apply.call(fn, this, arguments);
        if (result && typeof result.then === "function") {
          throw new TypeError("Transaction function cannot return a promise");
        }
        after.run();
        return result;
      } catch (ex) {
        if (db2.inTransaction) {
          undo.run();
          if (undo !== rollback) after.run();
        }
        throw ex;
      }
    };
  }
});

// ../node_modules/.store/better-sqlite3@12.2.0/node_modules/better-sqlite3/lib/methods/pragma.js
var require_pragma = __commonJS({
  "../node_modules/.store/better-sqlite3@12.2.0/node_modules/better-sqlite3/lib/methods/pragma.js"(exports2, module2) {
    "use strict";
    var { getBooleanOption, cppdb } = require_util();
    module2.exports = function pragma(source, options) {
      if (options == null) options = {};
      if (typeof source !== "string") throw new TypeError("Expected first argument to be a string");
      if (typeof options !== "object") throw new TypeError("Expected second argument to be an options object");
      const simple = getBooleanOption(options, "simple");
      const stmt = this[cppdb].prepare(`PRAGMA ${source}`, this, true);
      return simple ? stmt.pluck().get() : stmt.all();
    };
  }
});

// ../node_modules/.store/better-sqlite3@12.2.0/node_modules/better-sqlite3/lib/methods/backup.js
var require_backup = __commonJS({
  "../node_modules/.store/better-sqlite3@12.2.0/node_modules/better-sqlite3/lib/methods/backup.js"(exports2, module2) {
    "use strict";
    var fs2 = require("fs");
    var path2 = require("path");
    var { promisify } = require("util");
    var { cppdb } = require_util();
    var fsAccess = promisify(fs2.access);
    module2.exports = async function backup(filename, options) {
      if (options == null) options = {};
      if (typeof filename !== "string") throw new TypeError("Expected first argument to be a string");
      if (typeof options !== "object") throw new TypeError("Expected second argument to be an options object");
      filename = filename.trim();
      const attachedName = "attached" in options ? options.attached : "main";
      const handler = "progress" in options ? options.progress : null;
      if (!filename) throw new TypeError("Backup filename cannot be an empty string");
      if (filename === ":memory:") throw new TypeError('Invalid backup filename ":memory:"');
      if (typeof attachedName !== "string") throw new TypeError('Expected the "attached" option to be a string');
      if (!attachedName) throw new TypeError('The "attached" option cannot be an empty string');
      if (handler != null && typeof handler !== "function") throw new TypeError('Expected the "progress" option to be a function');
      await fsAccess(path2.dirname(filename)).catch(() => {
        throw new TypeError("Cannot save backup because the directory does not exist");
      });
      const isNewFile = await fsAccess(filename).then(() => false, () => true);
      return runBackup(this[cppdb].backup(this, attachedName, filename, isNewFile), handler || null);
    };
    var runBackup = (backup, handler) => {
      let rate = 0;
      let useDefault = true;
      return new Promise((resolve, reject) => {
        setImmediate(function step() {
          try {
            const progress = backup.transfer(rate);
            if (!progress.remainingPages) {
              backup.close();
              resolve(progress);
              return;
            }
            if (useDefault) {
              useDefault = false;
              rate = 100;
            }
            if (handler) {
              const ret = handler(progress);
              if (ret !== void 0) {
                if (typeof ret === "number" && ret === ret) rate = Math.max(0, Math.min(2147483647, Math.round(ret)));
                else throw new TypeError("Expected progress callback to return a number or undefined");
              }
            }
            setImmediate(step);
          } catch (err) {
            backup.close();
            reject(err);
          }
        });
      });
    };
  }
});

// ../node_modules/.store/better-sqlite3@12.2.0/node_modules/better-sqlite3/lib/methods/serialize.js
var require_serialize = __commonJS({
  "../node_modules/.store/better-sqlite3@12.2.0/node_modules/better-sqlite3/lib/methods/serialize.js"(exports2, module2) {
    "use strict";
    var { cppdb } = require_util();
    module2.exports = function serialize(options) {
      if (options == null) options = {};
      if (typeof options !== "object") throw new TypeError("Expected first argument to be an options object");
      const attachedName = "attached" in options ? options.attached : "main";
      if (typeof attachedName !== "string") throw new TypeError('Expected the "attached" option to be a string');
      if (!attachedName) throw new TypeError('The "attached" option cannot be an empty string');
      return this[cppdb].serialize(attachedName);
    };
  }
});

// ../node_modules/.store/better-sqlite3@12.2.0/node_modules/better-sqlite3/lib/methods/function.js
var require_function = __commonJS({
  "../node_modules/.store/better-sqlite3@12.2.0/node_modules/better-sqlite3/lib/methods/function.js"(exports2, module2) {
    "use strict";
    var { getBooleanOption, cppdb } = require_util();
    module2.exports = function defineFunction(name, options, fn) {
      if (options == null) options = {};
      if (typeof options === "function") {
        fn = options;
        options = {};
      }
      if (typeof name !== "string") throw new TypeError("Expected first argument to be a string");
      if (typeof fn !== "function") throw new TypeError("Expected last argument to be a function");
      if (typeof options !== "object") throw new TypeError("Expected second argument to be an options object");
      if (!name) throw new TypeError("User-defined function name cannot be an empty string");
      const safeIntegers = "safeIntegers" in options ? +getBooleanOption(options, "safeIntegers") : 2;
      const deterministic = getBooleanOption(options, "deterministic");
      const directOnly = getBooleanOption(options, "directOnly");
      const varargs = getBooleanOption(options, "varargs");
      let argCount = -1;
      if (!varargs) {
        argCount = fn.length;
        if (!Number.isInteger(argCount) || argCount < 0) throw new TypeError("Expected function.length to be a positive integer");
        if (argCount > 100) throw new RangeError("User-defined functions cannot have more than 100 arguments");
      }
      this[cppdb].function(fn, name, argCount, safeIntegers, deterministic, directOnly);
      return this;
    };
  }
});

// ../node_modules/.store/better-sqlite3@12.2.0/node_modules/better-sqlite3/lib/methods/aggregate.js
var require_aggregate = __commonJS({
  "../node_modules/.store/better-sqlite3@12.2.0/node_modules/better-sqlite3/lib/methods/aggregate.js"(exports2, module2) {
    "use strict";
    var { getBooleanOption, cppdb } = require_util();
    module2.exports = function defineAggregate(name, options) {
      if (typeof name !== "string") throw new TypeError("Expected first argument to be a string");
      if (typeof options !== "object" || options === null) throw new TypeError("Expected second argument to be an options object");
      if (!name) throw new TypeError("User-defined function name cannot be an empty string");
      const start = "start" in options ? options.start : null;
      const step = getFunctionOption(options, "step", true);
      const inverse = getFunctionOption(options, "inverse", false);
      const result = getFunctionOption(options, "result", false);
      const safeIntegers = "safeIntegers" in options ? +getBooleanOption(options, "safeIntegers") : 2;
      const deterministic = getBooleanOption(options, "deterministic");
      const directOnly = getBooleanOption(options, "directOnly");
      const varargs = getBooleanOption(options, "varargs");
      let argCount = -1;
      if (!varargs) {
        argCount = Math.max(getLength(step), inverse ? getLength(inverse) : 0);
        if (argCount > 0) argCount -= 1;
        if (argCount > 100) throw new RangeError("User-defined functions cannot have more than 100 arguments");
      }
      this[cppdb].aggregate(start, step, inverse, result, name, argCount, safeIntegers, deterministic, directOnly);
      return this;
    };
    var getFunctionOption = (options, key, required) => {
      const value = key in options ? options[key] : null;
      if (typeof value === "function") return value;
      if (value != null) throw new TypeError(`Expected the "${key}" option to be a function`);
      if (required) throw new TypeError(`Missing required option "${key}"`);
      return null;
    };
    var getLength = ({ length }) => {
      if (Number.isInteger(length) && length >= 0) return length;
      throw new TypeError("Expected function.length to be a positive integer");
    };
  }
});

// ../node_modules/.store/better-sqlite3@12.2.0/node_modules/better-sqlite3/lib/methods/table.js
var require_table = __commonJS({
  "../node_modules/.store/better-sqlite3@12.2.0/node_modules/better-sqlite3/lib/methods/table.js"(exports2, module2) {
    "use strict";
    var { cppdb } = require_util();
    module2.exports = function defineTable(name, factory) {
      if (typeof name !== "string") throw new TypeError("Expected first argument to be a string");
      if (!name) throw new TypeError("Virtual table module name cannot be an empty string");
      let eponymous = false;
      if (typeof factory === "object" && factory !== null) {
        eponymous = true;
        factory = defer(parseTableDefinition(factory, "used", name));
      } else {
        if (typeof factory !== "function") throw new TypeError("Expected second argument to be a function or a table definition object");
        factory = wrapFactory(factory);
      }
      this[cppdb].table(factory, name, eponymous);
      return this;
    };
    function wrapFactory(factory) {
      return function virtualTableFactory(moduleName, databaseName, tableName, ...args) {
        const thisObject = {
          module: moduleName,
          database: databaseName,
          table: tableName
        };
        const def = apply.call(factory, thisObject, args);
        if (typeof def !== "object" || def === null) {
          throw new TypeError(`Virtual table module "${moduleName}" did not return a table definition object`);
        }
        return parseTableDefinition(def, "returned", moduleName);
      };
    }
    function parseTableDefinition(def, verb, moduleName) {
      if (!hasOwnProperty.call(def, "rows")) {
        throw new TypeError(`Virtual table module "${moduleName}" ${verb} a table definition without a "rows" property`);
      }
      if (!hasOwnProperty.call(def, "columns")) {
        throw new TypeError(`Virtual table module "${moduleName}" ${verb} a table definition without a "columns" property`);
      }
      const rows = def.rows;
      if (typeof rows !== "function" || Object.getPrototypeOf(rows) !== GeneratorFunctionPrototype) {
        throw new TypeError(`Virtual table module "${moduleName}" ${verb} a table definition with an invalid "rows" property (should be a generator function)`);
      }
      let columns = def.columns;
      if (!Array.isArray(columns) || !(columns = [...columns]).every((x) => typeof x === "string")) {
        throw new TypeError(`Virtual table module "${moduleName}" ${verb} a table definition with an invalid "columns" property (should be an array of strings)`);
      }
      if (columns.length !== new Set(columns).size) {
        throw new TypeError(`Virtual table module "${moduleName}" ${verb} a table definition with duplicate column names`);
      }
      if (!columns.length) {
        throw new RangeError(`Virtual table module "${moduleName}" ${verb} a table definition with zero columns`);
      }
      let parameters;
      if (hasOwnProperty.call(def, "parameters")) {
        parameters = def.parameters;
        if (!Array.isArray(parameters) || !(parameters = [...parameters]).every((x) => typeof x === "string")) {
          throw new TypeError(`Virtual table module "${moduleName}" ${verb} a table definition with an invalid "parameters" property (should be an array of strings)`);
        }
      } else {
        parameters = inferParameters(rows);
      }
      if (parameters.length !== new Set(parameters).size) {
        throw new TypeError(`Virtual table module "${moduleName}" ${verb} a table definition with duplicate parameter names`);
      }
      if (parameters.length > 32) {
        throw new RangeError(`Virtual table module "${moduleName}" ${verb} a table definition with more than the maximum number of 32 parameters`);
      }
      for (const parameter of parameters) {
        if (columns.includes(parameter)) {
          throw new TypeError(`Virtual table module "${moduleName}" ${verb} a table definition with column "${parameter}" which was ambiguously defined as both a column and parameter`);
        }
      }
      let safeIntegers = 2;
      if (hasOwnProperty.call(def, "safeIntegers")) {
        const bool = def.safeIntegers;
        if (typeof bool !== "boolean") {
          throw new TypeError(`Virtual table module "${moduleName}" ${verb} a table definition with an invalid "safeIntegers" property (should be a boolean)`);
        }
        safeIntegers = +bool;
      }
      let directOnly = false;
      if (hasOwnProperty.call(def, "directOnly")) {
        directOnly = def.directOnly;
        if (typeof directOnly !== "boolean") {
          throw new TypeError(`Virtual table module "${moduleName}" ${verb} a table definition with an invalid "directOnly" property (should be a boolean)`);
        }
      }
      const columnDefinitions = [
        ...parameters.map(identifier).map((str) => `${str} HIDDEN`),
        ...columns.map(identifier)
      ];
      return [
        `CREATE TABLE x(${columnDefinitions.join(", ")});`,
        wrapGenerator(rows, new Map(columns.map((x, i) => [x, parameters.length + i])), moduleName),
        parameters,
        safeIntegers,
        directOnly
      ];
    }
    function wrapGenerator(generator, columnMap, moduleName) {
      return function* virtualTable(...args) {
        const output = args.map((x) => Buffer.isBuffer(x) ? Buffer.from(x) : x);
        for (let i = 0; i < columnMap.size; ++i) {
          output.push(null);
        }
        for (const row of generator(...args)) {
          if (Array.isArray(row)) {
            extractRowArray(row, output, columnMap.size, moduleName);
            yield output;
          } else if (typeof row === "object" && row !== null) {
            extractRowObject(row, output, columnMap, moduleName);
            yield output;
          } else {
            throw new TypeError(`Virtual table module "${moduleName}" yielded something that isn't a valid row object`);
          }
        }
      };
    }
    function extractRowArray(row, output, columnCount, moduleName) {
      if (row.length !== columnCount) {
        throw new TypeError(`Virtual table module "${moduleName}" yielded a row with an incorrect number of columns`);
      }
      const offset = output.length - columnCount;
      for (let i = 0; i < columnCount; ++i) {
        output[i + offset] = row[i];
      }
    }
    function extractRowObject(row, output, columnMap, moduleName) {
      let count = 0;
      for (const key of Object.keys(row)) {
        const index = columnMap.get(key);
        if (index === void 0) {
          throw new TypeError(`Virtual table module "${moduleName}" yielded a row with an undeclared column "${key}"`);
        }
        output[index] = row[key];
        count += 1;
      }
      if (count !== columnMap.size) {
        throw new TypeError(`Virtual table module "${moduleName}" yielded a row with missing columns`);
      }
    }
    function inferParameters({ length }) {
      if (!Number.isInteger(length) || length < 0) {
        throw new TypeError("Expected function.length to be a positive integer");
      }
      const params = [];
      for (let i = 0; i < length; ++i) {
        params.push(`$${i + 1}`);
      }
      return params;
    }
    var { hasOwnProperty } = Object.prototype;
    var { apply } = Function.prototype;
    var GeneratorFunctionPrototype = Object.getPrototypeOf(function* () {
    });
    var identifier = (str) => `"${str.replace(/"/g, '""')}"`;
    var defer = (x) => () => x;
  }
});

// ../node_modules/.store/better-sqlite3@12.2.0/node_modules/better-sqlite3/lib/methods/inspect.js
var require_inspect = __commonJS({
  "../node_modules/.store/better-sqlite3@12.2.0/node_modules/better-sqlite3/lib/methods/inspect.js"(exports2, module2) {
    "use strict";
    var DatabaseInspection = function Database2() {
    };
    module2.exports = function inspect(depth, opts) {
      return Object.assign(new DatabaseInspection(), this);
    };
  }
});

// ../node_modules/.store/better-sqlite3@12.2.0/node_modules/better-sqlite3/lib/database.js
var require_database = __commonJS({
  "../node_modules/.store/better-sqlite3@12.2.0/node_modules/better-sqlite3/lib/database.js"(exports2, module2) {
    "use strict";
    var fs2 = require("fs");
    var path2 = require("path");
    var util2 = require_util();
    var SqliteError = require_sqlite_error();
    var DEFAULT_ADDON;
    function Database2(filenameGiven, options) {
      if (new.target == null) {
        return new Database2(filenameGiven, options);
      }
      let buffer;
      if (Buffer.isBuffer(filenameGiven)) {
        buffer = filenameGiven;
        filenameGiven = ":memory:";
      }
      if (filenameGiven == null) filenameGiven = "";
      if (options == null) options = {};
      if (typeof filenameGiven !== "string") throw new TypeError("Expected first argument to be a string");
      if (typeof options !== "object") throw new TypeError("Expected second argument to be an options object");
      if ("readOnly" in options) throw new TypeError('Misspelled option "readOnly" should be "readonly"');
      if ("memory" in options) throw new TypeError('Option "memory" was removed in v7.0.0 (use ":memory:" filename instead)');
      const filename = filenameGiven.trim();
      const anonymous = filename === "" || filename === ":memory:";
      const readonly = util2.getBooleanOption(options, "readonly");
      const fileMustExist = util2.getBooleanOption(options, "fileMustExist");
      const timeout = "timeout" in options ? options.timeout : 5e3;
      const verbose = "verbose" in options ? options.verbose : null;
      const nativeBinding = "nativeBinding" in options ? options.nativeBinding : null;
      if (readonly && anonymous && !buffer) throw new TypeError("In-memory/temporary databases cannot be readonly");
      if (!Number.isInteger(timeout) || timeout < 0) throw new TypeError('Expected the "timeout" option to be a positive integer');
      if (timeout > 2147483647) throw new RangeError('Option "timeout" cannot be greater than 2147483647');
      if (verbose != null && typeof verbose !== "function") throw new TypeError('Expected the "verbose" option to be a function');
      if (nativeBinding != null && typeof nativeBinding !== "string" && typeof nativeBinding !== "object") throw new TypeError('Expected the "nativeBinding" option to be a string or addon object');
      let addon;
      if (nativeBinding == null) {
        addon = DEFAULT_ADDON || (DEFAULT_ADDON = require_bindings()("better_sqlite3.node"));
      } else if (typeof nativeBinding === "string") {
        const requireFunc = typeof __non_webpack_require__ === "function" ? __non_webpack_require__ : require;
        addon = requireFunc(path2.resolve(nativeBinding).replace(/(\.node)?$/, ".node"));
      } else {
        addon = nativeBinding;
      }
      if (!addon.isInitialized) {
        addon.setErrorConstructor(SqliteError);
        addon.isInitialized = true;
      }
      if (!anonymous && !fs2.existsSync(path2.dirname(filename))) {
        throw new TypeError("Cannot open database because the directory does not exist");
      }
      Object.defineProperties(this, {
        [util2.cppdb]: { value: new addon.Database(filename, filenameGiven, anonymous, readonly, fileMustExist, timeout, verbose || null, buffer || null) },
        ...wrappers.getters
      });
    }
    var wrappers = require_wrappers();
    Database2.prototype.prepare = wrappers.prepare;
    Database2.prototype.transaction = require_transaction();
    Database2.prototype.pragma = require_pragma();
    Database2.prototype.backup = require_backup();
    Database2.prototype.serialize = require_serialize();
    Database2.prototype.function = require_function();
    Database2.prototype.aggregate = require_aggregate();
    Database2.prototype.table = require_table();
    Database2.prototype.loadExtension = wrappers.loadExtension;
    Database2.prototype.exec = wrappers.exec;
    Database2.prototype.close = wrappers.close;
    Database2.prototype.defaultSafeIntegers = wrappers.defaultSafeIntegers;
    Database2.prototype.unsafeMode = wrappers.unsafeMode;
    Database2.prototype[util2.inspect] = require_inspect();
    module2.exports = Database2;
  }
});

// ../node_modules/.store/better-sqlite3@12.2.0/node_modules/better-sqlite3/lib/index.js
var require_lib = __commonJS({
  "../node_modules/.store/better-sqlite3@12.2.0/node_modules/better-sqlite3/lib/index.js"(exports2, module2) {
    "use strict";
    module2.exports = require_database();
    module2.exports.SqliteError = require_sqlite_error();
  }
});

// node_modules/.store/lru-cache@11.1.0/node_modules/lru-cache/dist/commonjs/index.js
var require_commonjs = __commonJS({
  "node_modules/.store/lru-cache@11.1.0/node_modules/lru-cache/dist/commonjs/index.js"(exports2) {
    "use strict";
    Object.defineProperty(exports2, "__esModule", { value: true });
    exports2.LRUCache = void 0;
    var perf = typeof performance === "object" && performance && typeof performance.now === "function" ? performance : Date;
    var warned = /* @__PURE__ */ new Set();
    var PROCESS = typeof process === "object" && !!process ? process : {};
    var emitWarning = (msg, type, code, fn) => {
      typeof PROCESS.emitWarning === "function" ? PROCESS.emitWarning(msg, type, code, fn) : console.error(`[${code}] ${type}: ${msg}`);
    };
    var AC = globalThis.AbortController;
    var AS = globalThis.AbortSignal;
    if (typeof AC === "undefined") {
      AS = class AbortSignal {
        onabort;
        _onabort = [];
        reason;
        aborted = false;
        addEventListener(_, fn) {
          this._onabort.push(fn);
        }
      };
      AC = class AbortController {
        constructor() {
          warnACPolyfill();
        }
        signal = new AS();
        abort(reason) {
          if (this.signal.aborted)
            return;
          this.signal.reason = reason;
          this.signal.aborted = true;
          for (const fn of this.signal._onabort) {
            fn(reason);
          }
          this.signal.onabort?.(reason);
        }
      };
      let printACPolyfillWarning = PROCESS.env?.LRU_CACHE_IGNORE_AC_WARNING !== "1";
      const warnACPolyfill = () => {
        if (!printACPolyfillWarning)
          return;
        printACPolyfillWarning = false;
        emitWarning("AbortController is not defined. If using lru-cache in node 14, load an AbortController polyfill from the `node-abort-controller` package. A minimal polyfill is provided for use by LRUCache.fetch(), but it should not be relied upon in other contexts (eg, passing it to other APIs that use AbortController/AbortSignal might have undesirable effects). You may disable this with LRU_CACHE_IGNORE_AC_WARNING=1 in the env.", "NO_ABORT_CONTROLLER", "ENOTSUP", warnACPolyfill);
      };
    }
    var shouldWarn = (code) => !warned.has(code);
    var TYPE = Symbol("type");
    var isPosInt = (n) => n && n === Math.floor(n) && n > 0 && isFinite(n);
    var getUintArray = (max) => !isPosInt(max) ? null : max <= Math.pow(2, 8) ? Uint8Array : max <= Math.pow(2, 16) ? Uint16Array : max <= Math.pow(2, 32) ? Uint32Array : max <= Number.MAX_SAFE_INTEGER ? ZeroArray : null;
    var ZeroArray = class extends Array {
      constructor(size) {
        super(size);
        this.fill(0);
      }
    };
    var Stack = class _Stack {
      heap;
      length;
      // private constructor
      static #constructing = false;
      static create(max) {
        const HeapCls = getUintArray(max);
        if (!HeapCls)
          return [];
        _Stack.#constructing = true;
        const s = new _Stack(max, HeapCls);
        _Stack.#constructing = false;
        return s;
      }
      constructor(max, HeapCls) {
        if (!_Stack.#constructing) {
          throw new TypeError("instantiate Stack using Stack.create(n)");
        }
        this.heap = new HeapCls(max);
        this.length = 0;
      }
      push(n) {
        this.heap[this.length++] = n;
      }
      pop() {
        return this.heap[--this.length];
      }
    };
    var LRUCache2 = class _LRUCache {
      // options that cannot be changed without disaster
      #max;
      #maxSize;
      #dispose;
      #onInsert;
      #disposeAfter;
      #fetchMethod;
      #memoMethod;
      /**
       * {@link LRUCache.OptionsBase.ttl}
       */
      ttl;
      /**
       * {@link LRUCache.OptionsBase.ttlResolution}
       */
      ttlResolution;
      /**
       * {@link LRUCache.OptionsBase.ttlAutopurge}
       */
      ttlAutopurge;
      /**
       * {@link LRUCache.OptionsBase.updateAgeOnGet}
       */
      updateAgeOnGet;
      /**
       * {@link LRUCache.OptionsBase.updateAgeOnHas}
       */
      updateAgeOnHas;
      /**
       * {@link LRUCache.OptionsBase.allowStale}
       */
      allowStale;
      /**
       * {@link LRUCache.OptionsBase.noDisposeOnSet}
       */
      noDisposeOnSet;
      /**
       * {@link LRUCache.OptionsBase.noUpdateTTL}
       */
      noUpdateTTL;
      /**
       * {@link LRUCache.OptionsBase.maxEntrySize}
       */
      maxEntrySize;
      /**
       * {@link LRUCache.OptionsBase.sizeCalculation}
       */
      sizeCalculation;
      /**
       * {@link LRUCache.OptionsBase.noDeleteOnFetchRejection}
       */
      noDeleteOnFetchRejection;
      /**
       * {@link LRUCache.OptionsBase.noDeleteOnStaleGet}
       */
      noDeleteOnStaleGet;
      /**
       * {@link LRUCache.OptionsBase.allowStaleOnFetchAbort}
       */
      allowStaleOnFetchAbort;
      /**
       * {@link LRUCache.OptionsBase.allowStaleOnFetchRejection}
       */
      allowStaleOnFetchRejection;
      /**
       * {@link LRUCache.OptionsBase.ignoreFetchAbort}
       */
      ignoreFetchAbort;
      // computed properties
      #size;
      #calculatedSize;
      #keyMap;
      #keyList;
      #valList;
      #next;
      #prev;
      #head;
      #tail;
      #free;
      #disposed;
      #sizes;
      #starts;
      #ttls;
      #hasDispose;
      #hasFetchMethod;
      #hasDisposeAfter;
      #hasOnInsert;
      /**
       * Do not call this method unless you need to inspect the
       * inner workings of the cache.  If anything returned by this
       * object is modified in any way, strange breakage may occur.
       *
       * These fields are private for a reason!
       *
       * @internal
       */
      static unsafeExposeInternals(c) {
        return {
          // properties
          starts: c.#starts,
          ttls: c.#ttls,
          sizes: c.#sizes,
          keyMap: c.#keyMap,
          keyList: c.#keyList,
          valList: c.#valList,
          next: c.#next,
          prev: c.#prev,
          get head() {
            return c.#head;
          },
          get tail() {
            return c.#tail;
          },
          free: c.#free,
          // methods
          isBackgroundFetch: (p) => c.#isBackgroundFetch(p),
          backgroundFetch: (k, index, options, context) => c.#backgroundFetch(k, index, options, context),
          moveToTail: (index) => c.#moveToTail(index),
          indexes: (options) => c.#indexes(options),
          rindexes: (options) => c.#rindexes(options),
          isStale: (index) => c.#isStale(index)
        };
      }
      // Protected read-only members
      /**
       * {@link LRUCache.OptionsBase.max} (read-only)
       */
      get max() {
        return this.#max;
      }
      /**
       * {@link LRUCache.OptionsBase.maxSize} (read-only)
       */
      get maxSize() {
        return this.#maxSize;
      }
      /**
       * The total computed size of items in the cache (read-only)
       */
      get calculatedSize() {
        return this.#calculatedSize;
      }
      /**
       * The number of items stored in the cache (read-only)
       */
      get size() {
        return this.#size;
      }
      /**
       * {@link LRUCache.OptionsBase.fetchMethod} (read-only)
       */
      get fetchMethod() {
        return this.#fetchMethod;
      }
      get memoMethod() {
        return this.#memoMethod;
      }
      /**
       * {@link LRUCache.OptionsBase.dispose} (read-only)
       */
      get dispose() {
        return this.#dispose;
      }
      /**
       * {@link LRUCache.OptionsBase.onInsert} (read-only)
       */
      get onInsert() {
        return this.#onInsert;
      }
      /**
       * {@link LRUCache.OptionsBase.disposeAfter} (read-only)
       */
      get disposeAfter() {
        return this.#disposeAfter;
      }
      constructor(options) {
        const { max = 0, ttl, ttlResolution = 1, ttlAutopurge, updateAgeOnGet, updateAgeOnHas, allowStale, dispose, onInsert, disposeAfter, noDisposeOnSet, noUpdateTTL, maxSize = 0, maxEntrySize = 0, sizeCalculation, fetchMethod, memoMethod, noDeleteOnFetchRejection, noDeleteOnStaleGet, allowStaleOnFetchRejection, allowStaleOnFetchAbort, ignoreFetchAbort } = options;
        if (max !== 0 && !isPosInt(max)) {
          throw new TypeError("max option must be a nonnegative integer");
        }
        const UintArray = max ? getUintArray(max) : Array;
        if (!UintArray) {
          throw new Error("invalid max value: " + max);
        }
        this.#max = max;
        this.#maxSize = maxSize;
        this.maxEntrySize = maxEntrySize || this.#maxSize;
        this.sizeCalculation = sizeCalculation;
        if (this.sizeCalculation) {
          if (!this.#maxSize && !this.maxEntrySize) {
            throw new TypeError("cannot set sizeCalculation without setting maxSize or maxEntrySize");
          }
          if (typeof this.sizeCalculation !== "function") {
            throw new TypeError("sizeCalculation set to non-function");
          }
        }
        if (memoMethod !== void 0 && typeof memoMethod !== "function") {
          throw new TypeError("memoMethod must be a function if defined");
        }
        this.#memoMethod = memoMethod;
        if (fetchMethod !== void 0 && typeof fetchMethod !== "function") {
          throw new TypeError("fetchMethod must be a function if specified");
        }
        this.#fetchMethod = fetchMethod;
        this.#hasFetchMethod = !!fetchMethod;
        this.#keyMap = /* @__PURE__ */ new Map();
        this.#keyList = new Array(max).fill(void 0);
        this.#valList = new Array(max).fill(void 0);
        this.#next = new UintArray(max);
        this.#prev = new UintArray(max);
        this.#head = 0;
        this.#tail = 0;
        this.#free = Stack.create(max);
        this.#size = 0;
        this.#calculatedSize = 0;
        if (typeof dispose === "function") {
          this.#dispose = dispose;
        }
        if (typeof onInsert === "function") {
          this.#onInsert = onInsert;
        }
        if (typeof disposeAfter === "function") {
          this.#disposeAfter = disposeAfter;
          this.#disposed = [];
        } else {
          this.#disposeAfter = void 0;
          this.#disposed = void 0;
        }
        this.#hasDispose = !!this.#dispose;
        this.#hasOnInsert = !!this.#onInsert;
        this.#hasDisposeAfter = !!this.#disposeAfter;
        this.noDisposeOnSet = !!noDisposeOnSet;
        this.noUpdateTTL = !!noUpdateTTL;
        this.noDeleteOnFetchRejection = !!noDeleteOnFetchRejection;
        this.allowStaleOnFetchRejection = !!allowStaleOnFetchRejection;
        this.allowStaleOnFetchAbort = !!allowStaleOnFetchAbort;
        this.ignoreFetchAbort = !!ignoreFetchAbort;
        if (this.maxEntrySize !== 0) {
          if (this.#maxSize !== 0) {
            if (!isPosInt(this.#maxSize)) {
              throw new TypeError("maxSize must be a positive integer if specified");
            }
          }
          if (!isPosInt(this.maxEntrySize)) {
            throw new TypeError("maxEntrySize must be a positive integer if specified");
          }
          this.#initializeSizeTracking();
        }
        this.allowStale = !!allowStale;
        this.noDeleteOnStaleGet = !!noDeleteOnStaleGet;
        this.updateAgeOnGet = !!updateAgeOnGet;
        this.updateAgeOnHas = !!updateAgeOnHas;
        this.ttlResolution = isPosInt(ttlResolution) || ttlResolution === 0 ? ttlResolution : 1;
        this.ttlAutopurge = !!ttlAutopurge;
        this.ttl = ttl || 0;
        if (this.ttl) {
          if (!isPosInt(this.ttl)) {
            throw new TypeError("ttl must be a positive integer if specified");
          }
          this.#initializeTTLTracking();
        }
        if (this.#max === 0 && this.ttl === 0 && this.#maxSize === 0) {
          throw new TypeError("At least one of max, maxSize, or ttl is required");
        }
        if (!this.ttlAutopurge && !this.#max && !this.#maxSize) {
          const code = "LRU_CACHE_UNBOUNDED";
          if (shouldWarn(code)) {
            warned.add(code);
            const msg = "TTL caching without ttlAutopurge, max, or maxSize can result in unbounded memory consumption.";
            emitWarning(msg, "UnboundedCacheWarning", code, _LRUCache);
          }
        }
      }
      /**
       * Return the number of ms left in the item's TTL. If item is not in cache,
       * returns `0`. Returns `Infinity` if item is in cache without a defined TTL.
       */
      getRemainingTTL(key) {
        return this.#keyMap.has(key) ? Infinity : 0;
      }
      #initializeTTLTracking() {
        const ttls = new ZeroArray(this.#max);
        const starts = new ZeroArray(this.#max);
        this.#ttls = ttls;
        this.#starts = starts;
        this.#setItemTTL = (index, ttl, start = perf.now()) => {
          starts[index] = ttl !== 0 ? start : 0;
          ttls[index] = ttl;
          if (ttl !== 0 && this.ttlAutopurge) {
            const t = setTimeout(() => {
              if (this.#isStale(index)) {
                this.#delete(this.#keyList[index], "expire");
              }
            }, ttl + 1);
            if (t.unref) {
              t.unref();
            }
          }
        };
        this.#updateItemAge = (index) => {
          starts[index] = ttls[index] !== 0 ? perf.now() : 0;
        };
        this.#statusTTL = (status, index) => {
          if (ttls[index]) {
            const ttl = ttls[index];
            const start = starts[index];
            if (!ttl || !start)
              return;
            status.ttl = ttl;
            status.start = start;
            status.now = cachedNow || getNow();
            const age = status.now - start;
            status.remainingTTL = ttl - age;
          }
        };
        let cachedNow = 0;
        const getNow = () => {
          const n = perf.now();
          if (this.ttlResolution > 0) {
            cachedNow = n;
            const t = setTimeout(() => cachedNow = 0, this.ttlResolution);
            if (t.unref) {
              t.unref();
            }
          }
          return n;
        };
        this.getRemainingTTL = (key) => {
          const index = this.#keyMap.get(key);
          if (index === void 0) {
            return 0;
          }
          const ttl = ttls[index];
          const start = starts[index];
          if (!ttl || !start) {
            return Infinity;
          }
          const age = (cachedNow || getNow()) - start;
          return ttl - age;
        };
        this.#isStale = (index) => {
          const s = starts[index];
          const t = ttls[index];
          return !!t && !!s && (cachedNow || getNow()) - s > t;
        };
      }
      // conditionally set private methods related to TTL
      #updateItemAge = () => {
      };
      #statusTTL = () => {
      };
      #setItemTTL = () => {
      };
      /* c8 ignore stop */
      #isStale = () => false;
      #initializeSizeTracking() {
        const sizes = new ZeroArray(this.#max);
        this.#calculatedSize = 0;
        this.#sizes = sizes;
        this.#removeItemSize = (index) => {
          this.#calculatedSize -= sizes[index];
          sizes[index] = 0;
        };
        this.#requireSize = (k, v, size, sizeCalculation) => {
          if (this.#isBackgroundFetch(v)) {
            return 0;
          }
          if (!isPosInt(size)) {
            if (sizeCalculation) {
              if (typeof sizeCalculation !== "function") {
                throw new TypeError("sizeCalculation must be a function");
              }
              size = sizeCalculation(v, k);
              if (!isPosInt(size)) {
                throw new TypeError("sizeCalculation return invalid (expect positive integer)");
              }
            } else {
              throw new TypeError("invalid size value (must be positive integer). When maxSize or maxEntrySize is used, sizeCalculation or size must be set.");
            }
          }
          return size;
        };
        this.#addItemSize = (index, size, status) => {
          sizes[index] = size;
          if (this.#maxSize) {
            const maxSize = this.#maxSize - sizes[index];
            while (this.#calculatedSize > maxSize) {
              this.#evict(true);
            }
          }
          this.#calculatedSize += sizes[index];
          if (status) {
            status.entrySize = size;
            status.totalCalculatedSize = this.#calculatedSize;
          }
        };
      }
      #removeItemSize = (_i) => {
      };
      #addItemSize = (_i, _s, _st) => {
      };
      #requireSize = (_k, _v, size, sizeCalculation) => {
        if (size || sizeCalculation) {
          throw new TypeError("cannot set size without setting maxSize or maxEntrySize on cache");
        }
        return 0;
      };
      *#indexes({ allowStale = this.allowStale } = {}) {
        if (this.#size) {
          for (let i = this.#tail; true; ) {
            if (!this.#isValidIndex(i)) {
              break;
            }
            if (allowStale || !this.#isStale(i)) {
              yield i;
            }
            if (i === this.#head) {
              break;
            } else {
              i = this.#prev[i];
            }
          }
        }
      }
      *#rindexes({ allowStale = this.allowStale } = {}) {
        if (this.#size) {
          for (let i = this.#head; true; ) {
            if (!this.#isValidIndex(i)) {
              break;
            }
            if (allowStale || !this.#isStale(i)) {
              yield i;
            }
            if (i === this.#tail) {
              break;
            } else {
              i = this.#next[i];
            }
          }
        }
      }
      #isValidIndex(index) {
        return index !== void 0 && this.#keyMap.get(this.#keyList[index]) === index;
      }
      /**
       * Return a generator yielding `[key, value]` pairs,
       * in order from most recently used to least recently used.
       */
      *entries() {
        for (const i of this.#indexes()) {
          if (this.#valList[i] !== void 0 && this.#keyList[i] !== void 0 && !this.#isBackgroundFetch(this.#valList[i])) {
            yield [this.#keyList[i], this.#valList[i]];
          }
        }
      }
      /**
       * Inverse order version of {@link LRUCache.entries}
       *
       * Return a generator yielding `[key, value]` pairs,
       * in order from least recently used to most recently used.
       */
      *rentries() {
        for (const i of this.#rindexes()) {
          if (this.#valList[i] !== void 0 && this.#keyList[i] !== void 0 && !this.#isBackgroundFetch(this.#valList[i])) {
            yield [this.#keyList[i], this.#valList[i]];
          }
        }
      }
      /**
       * Return a generator yielding the keys in the cache,
       * in order from most recently used to least recently used.
       */
      *keys() {
        for (const i of this.#indexes()) {
          const k = this.#keyList[i];
          if (k !== void 0 && !this.#isBackgroundFetch(this.#valList[i])) {
            yield k;
          }
        }
      }
      /**
       * Inverse order version of {@link LRUCache.keys}
       *
       * Return a generator yielding the keys in the cache,
       * in order from least recently used to most recently used.
       */
      *rkeys() {
        for (const i of this.#rindexes()) {
          const k = this.#keyList[i];
          if (k !== void 0 && !this.#isBackgroundFetch(this.#valList[i])) {
            yield k;
          }
        }
      }
      /**
       * Return a generator yielding the values in the cache,
       * in order from most recently used to least recently used.
       */
      *values() {
        for (const i of this.#indexes()) {
          const v = this.#valList[i];
          if (v !== void 0 && !this.#isBackgroundFetch(this.#valList[i])) {
            yield this.#valList[i];
          }
        }
      }
      /**
       * Inverse order version of {@link LRUCache.values}
       *
       * Return a generator yielding the values in the cache,
       * in order from least recently used to most recently used.
       */
      *rvalues() {
        for (const i of this.#rindexes()) {
          const v = this.#valList[i];
          if (v !== void 0 && !this.#isBackgroundFetch(this.#valList[i])) {
            yield this.#valList[i];
          }
        }
      }
      /**
       * Iterating over the cache itself yields the same results as
       * {@link LRUCache.entries}
       */
      [Symbol.iterator]() {
        return this.entries();
      }
      /**
       * A String value that is used in the creation of the default string
       * description of an object. Called by the built-in method
       * `Object.prototype.toString`.
       */
      [Symbol.toStringTag] = "LRUCache";
      /**
       * Find a value for which the supplied fn method returns a truthy value,
       * similar to `Array.find()`. fn is called as `fn(value, key, cache)`.
       */
      find(fn, getOptions = {}) {
        for (const i of this.#indexes()) {
          const v = this.#valList[i];
          const value = this.#isBackgroundFetch(v) ? v.__staleWhileFetching : v;
          if (value === void 0)
            continue;
          if (fn(value, this.#keyList[i], this)) {
            return this.get(this.#keyList[i], getOptions);
          }
        }
      }
      /**
       * Call the supplied function on each item in the cache, in order from most
       * recently used to least recently used.
       *
       * `fn` is called as `fn(value, key, cache)`.
       *
       * If `thisp` is provided, function will be called in the `this`-context of
       * the provided object, or the cache if no `thisp` object is provided.
       *
       * Does not update age or recenty of use, or iterate over stale values.
       */
      forEach(fn, thisp = this) {
        for (const i of this.#indexes()) {
          const v = this.#valList[i];
          const value = this.#isBackgroundFetch(v) ? v.__staleWhileFetching : v;
          if (value === void 0)
            continue;
          fn.call(thisp, value, this.#keyList[i], this);
        }
      }
      /**
       * The same as {@link LRUCache.forEach} but items are iterated over in
       * reverse order.  (ie, less recently used items are iterated over first.)
       */
      rforEach(fn, thisp = this) {
        for (const i of this.#rindexes()) {
          const v = this.#valList[i];
          const value = this.#isBackgroundFetch(v) ? v.__staleWhileFetching : v;
          if (value === void 0)
            continue;
          fn.call(thisp, value, this.#keyList[i], this);
        }
      }
      /**
       * Delete any stale entries. Returns true if anything was removed,
       * false otherwise.
       */
      purgeStale() {
        let deleted = false;
        for (const i of this.#rindexes({ allowStale: true })) {
          if (this.#isStale(i)) {
            this.#delete(this.#keyList[i], "expire");
            deleted = true;
          }
        }
        return deleted;
      }
      /**
       * Get the extended info about a given entry, to get its value, size, and
       * TTL info simultaneously. Returns `undefined` if the key is not present.
       *
       * Unlike {@link LRUCache#dump}, which is designed to be portable and survive
       * serialization, the `start` value is always the current timestamp, and the
       * `ttl` is a calculated remaining time to live (negative if expired).
       *
       * Always returns stale values, if their info is found in the cache, so be
       * sure to check for expirations (ie, a negative {@link LRUCache.Entry#ttl})
       * if relevant.
       */
      info(key) {
        const i = this.#keyMap.get(key);
        if (i === void 0)
          return void 0;
        const v = this.#valList[i];
        const value = this.#isBackgroundFetch(v) ? v.__staleWhileFetching : v;
        if (value === void 0)
          return void 0;
        const entry = { value };
        if (this.#ttls && this.#starts) {
          const ttl = this.#ttls[i];
          const start = this.#starts[i];
          if (ttl && start) {
            const remain = ttl - (perf.now() - start);
            entry.ttl = remain;
            entry.start = Date.now();
          }
        }
        if (this.#sizes) {
          entry.size = this.#sizes[i];
        }
        return entry;
      }
      /**
       * Return an array of [key, {@link LRUCache.Entry}] tuples which can be
       * passed to {@link LRUCache#load}.
       *
       * The `start` fields are calculated relative to a portable `Date.now()`
       * timestamp, even if `performance.now()` is available.
       *
       * Stale entries are always included in the `dump`, even if
       * {@link LRUCache.OptionsBase.allowStale} is false.
       *
       * Note: this returns an actual array, not a generator, so it can be more
       * easily passed around.
       */
      dump() {
        const arr = [];
        for (const i of this.#indexes({ allowStale: true })) {
          const key = this.#keyList[i];
          const v = this.#valList[i];
          const value = this.#isBackgroundFetch(v) ? v.__staleWhileFetching : v;
          if (value === void 0 || key === void 0)
            continue;
          const entry = { value };
          if (this.#ttls && this.#starts) {
            entry.ttl = this.#ttls[i];
            const age = perf.now() - this.#starts[i];
            entry.start = Math.floor(Date.now() - age);
          }
          if (this.#sizes) {
            entry.size = this.#sizes[i];
          }
          arr.unshift([key, entry]);
        }
        return arr;
      }
      /**
       * Reset the cache and load in the items in entries in the order listed.
       *
       * The shape of the resulting cache may be different if the same options are
       * not used in both caches.
       *
       * The `start` fields are assumed to be calculated relative to a portable
       * `Date.now()` timestamp, even if `performance.now()` is available.
       */
      load(arr) {
        this.clear();
        for (const [key, entry] of arr) {
          if (entry.start) {
            const age = Date.now() - entry.start;
            entry.start = perf.now() - age;
          }
          this.set(key, entry.value, entry);
        }
      }
      /**
       * Add a value to the cache.
       *
       * Note: if `undefined` is specified as a value, this is an alias for
       * {@link LRUCache#delete}
       *
       * Fields on the {@link LRUCache.SetOptions} options param will override
       * their corresponding values in the constructor options for the scope
       * of this single `set()` operation.
       *
       * If `start` is provided, then that will set the effective start
       * time for the TTL calculation. Note that this must be a previous
       * value of `performance.now()` if supported, or a previous value of
       * `Date.now()` if not.
       *
       * Options object may also include `size`, which will prevent
       * calling the `sizeCalculation` function and just use the specified
       * number if it is a positive integer, and `noDisposeOnSet` which
       * will prevent calling a `dispose` function in the case of
       * overwrites.
       *
       * If the `size` (or return value of `sizeCalculation`) for a given
       * entry is greater than `maxEntrySize`, then the item will not be
       * added to the cache.
       *
       * Will update the recency of the entry.
       *
       * If the value is `undefined`, then this is an alias for
       * `cache.delete(key)`. `undefined` is never stored in the cache.
       */
      set(k, v, setOptions = {}) {
        if (v === void 0) {
          this.delete(k);
          return this;
        }
        const { ttl = this.ttl, start, noDisposeOnSet = this.noDisposeOnSet, sizeCalculation = this.sizeCalculation, status } = setOptions;
        let { noUpdateTTL = this.noUpdateTTL } = setOptions;
        const size = this.#requireSize(k, v, setOptions.size || 0, sizeCalculation);
        if (this.maxEntrySize && size > this.maxEntrySize) {
          if (status) {
            status.set = "miss";
            status.maxEntrySizeExceeded = true;
          }
          this.#delete(k, "set");
          return this;
        }
        let index = this.#size === 0 ? void 0 : this.#keyMap.get(k);
        if (index === void 0) {
          index = this.#size === 0 ? this.#tail : this.#free.length !== 0 ? this.#free.pop() : this.#size === this.#max ? this.#evict(false) : this.#size;
          this.#keyList[index] = k;
          this.#valList[index] = v;
          this.#keyMap.set(k, index);
          this.#next[this.#tail] = index;
          this.#prev[index] = this.#tail;
          this.#tail = index;
          this.#size++;
          this.#addItemSize(index, size, status);
          if (status)
            status.set = "add";
          noUpdateTTL = false;
          if (this.#hasOnInsert) {
            this.#onInsert?.(v, k, "add");
          }
        } else {
          this.#moveToTail(index);
          const oldVal = this.#valList[index];
          if (v !== oldVal) {
            if (this.#hasFetchMethod && this.#isBackgroundFetch(oldVal)) {
              oldVal.__abortController.abort(new Error("replaced"));
              const { __staleWhileFetching: s } = oldVal;
              if (s !== void 0 && !noDisposeOnSet) {
                if (this.#hasDispose) {
                  this.#dispose?.(s, k, "set");
                }
                if (this.#hasDisposeAfter) {
                  this.#disposed?.push([s, k, "set"]);
                }
              }
            } else if (!noDisposeOnSet) {
              if (this.#hasDispose) {
                this.#dispose?.(oldVal, k, "set");
              }
              if (this.#hasDisposeAfter) {
                this.#disposed?.push([oldVal, k, "set"]);
              }
            }
            this.#removeItemSize(index);
            this.#addItemSize(index, size, status);
            this.#valList[index] = v;
            if (status) {
              status.set = "replace";
              const oldValue = oldVal && this.#isBackgroundFetch(oldVal) ? oldVal.__staleWhileFetching : oldVal;
              if (oldValue !== void 0)
                status.oldValue = oldValue;
            }
          } else if (status) {
            status.set = "update";
          }
          if (this.#hasOnInsert) {
            this.onInsert?.(v, k, v === oldVal ? "update" : "replace");
          }
        }
        if (ttl !== 0 && !this.#ttls) {
          this.#initializeTTLTracking();
        }
        if (this.#ttls) {
          if (!noUpdateTTL) {
            this.#setItemTTL(index, ttl, start);
          }
          if (status)
            this.#statusTTL(status, index);
        }
        if (!noDisposeOnSet && this.#hasDisposeAfter && this.#disposed) {
          const dt = this.#disposed;
          let task;
          while (task = dt?.shift()) {
            this.#disposeAfter?.(...task);
          }
        }
        return this;
      }
      /**
       * Evict the least recently used item, returning its value or
       * `undefined` if cache is empty.
       */
      pop() {
        try {
          while (this.#size) {
            const val = this.#valList[this.#head];
            this.#evict(true);
            if (this.#isBackgroundFetch(val)) {
              if (val.__staleWhileFetching) {
                return val.__staleWhileFetching;
              }
            } else if (val !== void 0) {
              return val;
            }
          }
        } finally {
          if (this.#hasDisposeAfter && this.#disposed) {
            const dt = this.#disposed;
            let task;
            while (task = dt?.shift()) {
              this.#disposeAfter?.(...task);
            }
          }
        }
      }
      #evict(free) {
        const head = this.#head;
        const k = this.#keyList[head];
        const v = this.#valList[head];
        if (this.#hasFetchMethod && this.#isBackgroundFetch(v)) {
          v.__abortController.abort(new Error("evicted"));
        } else if (this.#hasDispose || this.#hasDisposeAfter) {
          if (this.#hasDispose) {
            this.#dispose?.(v, k, "evict");
          }
          if (this.#hasDisposeAfter) {
            this.#disposed?.push([v, k, "evict"]);
          }
        }
        this.#removeItemSize(head);
        if (free) {
          this.#keyList[head] = void 0;
          this.#valList[head] = void 0;
          this.#free.push(head);
        }
        if (this.#size === 1) {
          this.#head = this.#tail = 0;
          this.#free.length = 0;
        } else {
          this.#head = this.#next[head];
        }
        this.#keyMap.delete(k);
        this.#size--;
        return head;
      }
      /**
       * Check if a key is in the cache, without updating the recency of use.
       * Will return false if the item is stale, even though it is technically
       * in the cache.
       *
       * Check if a key is in the cache, without updating the recency of
       * use. Age is updated if {@link LRUCache.OptionsBase.updateAgeOnHas} is set
       * to `true` in either the options or the constructor.
       *
       * Will return `false` if the item is stale, even though it is technically in
       * the cache. The difference can be determined (if it matters) by using a
       * `status` argument, and inspecting the `has` field.
       *
       * Will not update item age unless
       * {@link LRUCache.OptionsBase.updateAgeOnHas} is set.
       */
      has(k, hasOptions = {}) {
        const { updateAgeOnHas = this.updateAgeOnHas, status } = hasOptions;
        const index = this.#keyMap.get(k);
        if (index !== void 0) {
          const v = this.#valList[index];
          if (this.#isBackgroundFetch(v) && v.__staleWhileFetching === void 0) {
            return false;
          }
          if (!this.#isStale(index)) {
            if (updateAgeOnHas) {
              this.#updateItemAge(index);
            }
            if (status) {
              status.has = "hit";
              this.#statusTTL(status, index);
            }
            return true;
          } else if (status) {
            status.has = "stale";
            this.#statusTTL(status, index);
          }
        } else if (status) {
          status.has = "miss";
        }
        return false;
      }
      /**
       * Like {@link LRUCache#get} but doesn't update recency or delete stale
       * items.
       *
       * Returns `undefined` if the item is stale, unless
       * {@link LRUCache.OptionsBase.allowStale} is set.
       */
      peek(k, peekOptions = {}) {
        const { allowStale = this.allowStale } = peekOptions;
        const index = this.#keyMap.get(k);
        if (index === void 0 || !allowStale && this.#isStale(index)) {
          return;
        }
        const v = this.#valList[index];
        return this.#isBackgroundFetch(v) ? v.__staleWhileFetching : v;
      }
      #backgroundFetch(k, index, options, context) {
        const v = index === void 0 ? void 0 : this.#valList[index];
        if (this.#isBackgroundFetch(v)) {
          return v;
        }
        const ac = new AC();
        const { signal } = options;
        signal?.addEventListener("abort", () => ac.abort(signal.reason), {
          signal: ac.signal
        });
        const fetchOpts = {
          signal: ac.signal,
          options,
          context
        };
        const cb = (v2, updateCache = false) => {
          const { aborted } = ac.signal;
          const ignoreAbort = options.ignoreFetchAbort && v2 !== void 0;
          if (options.status) {
            if (aborted && !updateCache) {
              options.status.fetchAborted = true;
              options.status.fetchError = ac.signal.reason;
              if (ignoreAbort)
                options.status.fetchAbortIgnored = true;
            } else {
              options.status.fetchResolved = true;
            }
          }
          if (aborted && !ignoreAbort && !updateCache) {
            return fetchFail(ac.signal.reason);
          }
          const bf2 = p;
          if (this.#valList[index] === p) {
            if (v2 === void 0) {
              if (bf2.__staleWhileFetching) {
                this.#valList[index] = bf2.__staleWhileFetching;
              } else {
                this.#delete(k, "fetch");
              }
            } else {
              if (options.status)
                options.status.fetchUpdated = true;
              this.set(k, v2, fetchOpts.options);
            }
          }
          return v2;
        };
        const eb = (er) => {
          if (options.status) {
            options.status.fetchRejected = true;
            options.status.fetchError = er;
          }
          return fetchFail(er);
        };
        const fetchFail = (er) => {
          const { aborted } = ac.signal;
          const allowStaleAborted = aborted && options.allowStaleOnFetchAbort;
          const allowStale = allowStaleAborted || options.allowStaleOnFetchRejection;
          const noDelete = allowStale || options.noDeleteOnFetchRejection;
          const bf2 = p;
          if (this.#valList[index] === p) {
            const del = !noDelete || bf2.__staleWhileFetching === void 0;
            if (del) {
              this.#delete(k, "fetch");
            } else if (!allowStaleAborted) {
              this.#valList[index] = bf2.__staleWhileFetching;
            }
          }
          if (allowStale) {
            if (options.status && bf2.__staleWhileFetching !== void 0) {
              options.status.returnedStale = true;
            }
            return bf2.__staleWhileFetching;
          } else if (bf2.__returned === bf2) {
            throw er;
          }
        };
        const pcall = (res, rej) => {
          const fmp = this.#fetchMethod?.(k, v, fetchOpts);
          if (fmp && fmp instanceof Promise) {
            fmp.then((v2) => res(v2 === void 0 ? void 0 : v2), rej);
          }
          ac.signal.addEventListener("abort", () => {
            if (!options.ignoreFetchAbort || options.allowStaleOnFetchAbort) {
              res(void 0);
              if (options.allowStaleOnFetchAbort) {
                res = (v2) => cb(v2, true);
              }
            }
          });
        };
        if (options.status)
          options.status.fetchDispatched = true;
        const p = new Promise(pcall).then(cb, eb);
        const bf = Object.assign(p, {
          __abortController: ac,
          __staleWhileFetching: v,
          __returned: void 0
        });
        if (index === void 0) {
          this.set(k, bf, { ...fetchOpts.options, status: void 0 });
          index = this.#keyMap.get(k);
        } else {
          this.#valList[index] = bf;
        }
        return bf;
      }
      #isBackgroundFetch(p) {
        if (!this.#hasFetchMethod)
          return false;
        const b = p;
        return !!b && b instanceof Promise && b.hasOwnProperty("__staleWhileFetching") && b.__abortController instanceof AC;
      }
      async fetch(k, fetchOptions = {}) {
        const {
          // get options
          allowStale = this.allowStale,
          updateAgeOnGet = this.updateAgeOnGet,
          noDeleteOnStaleGet = this.noDeleteOnStaleGet,
          // set options
          ttl = this.ttl,
          noDisposeOnSet = this.noDisposeOnSet,
          size = 0,
          sizeCalculation = this.sizeCalculation,
          noUpdateTTL = this.noUpdateTTL,
          // fetch exclusive options
          noDeleteOnFetchRejection = this.noDeleteOnFetchRejection,
          allowStaleOnFetchRejection = this.allowStaleOnFetchRejection,
          ignoreFetchAbort = this.ignoreFetchAbort,
          allowStaleOnFetchAbort = this.allowStaleOnFetchAbort,
          context,
          forceRefresh = false,
          status,
          signal
        } = fetchOptions;
        if (!this.#hasFetchMethod) {
          if (status)
            status.fetch = "get";
          return this.get(k, {
            allowStale,
            updateAgeOnGet,
            noDeleteOnStaleGet,
            status
          });
        }
        const options = {
          allowStale,
          updateAgeOnGet,
          noDeleteOnStaleGet,
          ttl,
          noDisposeOnSet,
          size,
          sizeCalculation,
          noUpdateTTL,
          noDeleteOnFetchRejection,
          allowStaleOnFetchRejection,
          allowStaleOnFetchAbort,
          ignoreFetchAbort,
          status,
          signal
        };
        let index = this.#keyMap.get(k);
        if (index === void 0) {
          if (status)
            status.fetch = "miss";
          const p = this.#backgroundFetch(k, index, options, context);
          return p.__returned = p;
        } else {
          const v = this.#valList[index];
          if (this.#isBackgroundFetch(v)) {
            const stale = allowStale && v.__staleWhileFetching !== void 0;
            if (status) {
              status.fetch = "inflight";
              if (stale)
                status.returnedStale = true;
            }
            return stale ? v.__staleWhileFetching : v.__returned = v;
          }
          const isStale = this.#isStale(index);
          if (!forceRefresh && !isStale) {
            if (status)
              status.fetch = "hit";
            this.#moveToTail(index);
            if (updateAgeOnGet) {
              this.#updateItemAge(index);
            }
            if (status)
              this.#statusTTL(status, index);
            return v;
          }
          const p = this.#backgroundFetch(k, index, options, context);
          const hasStale = p.__staleWhileFetching !== void 0;
          const staleVal = hasStale && allowStale;
          if (status) {
            status.fetch = isStale ? "stale" : "refresh";
            if (staleVal && isStale)
              status.returnedStale = true;
          }
          return staleVal ? p.__staleWhileFetching : p.__returned = p;
        }
      }
      async forceFetch(k, fetchOptions = {}) {
        const v = await this.fetch(k, fetchOptions);
        if (v === void 0)
          throw new Error("fetch() returned undefined");
        return v;
      }
      memo(k, memoOptions = {}) {
        const memoMethod = this.#memoMethod;
        if (!memoMethod) {
          throw new Error("no memoMethod provided to constructor");
        }
        const { context, forceRefresh, ...options } = memoOptions;
        const v = this.get(k, options);
        if (!forceRefresh && v !== void 0)
          return v;
        const vv = memoMethod(k, v, {
          options,
          context
        });
        this.set(k, vv, options);
        return vv;
      }
      /**
       * Return a value from the cache. Will update the recency of the cache
       * entry found.
       *
       * If the key is not found, get() will return `undefined`.
       */
      get(k, getOptions = {}) {
        const { allowStale = this.allowStale, updateAgeOnGet = this.updateAgeOnGet, noDeleteOnStaleGet = this.noDeleteOnStaleGet, status } = getOptions;
        const index = this.#keyMap.get(k);
        if (index !== void 0) {
          const value = this.#valList[index];
          const fetching = this.#isBackgroundFetch(value);
          if (status)
            this.#statusTTL(status, index);
          if (this.#isStale(index)) {
            if (status)
              status.get = "stale";
            if (!fetching) {
              if (!noDeleteOnStaleGet) {
                this.#delete(k, "expire");
              }
              if (status && allowStale)
                status.returnedStale = true;
              return allowStale ? value : void 0;
            } else {
              if (status && allowStale && value.__staleWhileFetching !== void 0) {
                status.returnedStale = true;
              }
              return allowStale ? value.__staleWhileFetching : void 0;
            }
          } else {
            if (status)
              status.get = "hit";
            if (fetching) {
              return value.__staleWhileFetching;
            }
            this.#moveToTail(index);
            if (updateAgeOnGet) {
              this.#updateItemAge(index);
            }
            return value;
          }
        } else if (status) {
          status.get = "miss";
        }
      }
      #connect(p, n) {
        this.#prev[n] = p;
        this.#next[p] = n;
      }
      #moveToTail(index) {
        if (index !== this.#tail) {
          if (index === this.#head) {
            this.#head = this.#next[index];
          } else {
            this.#connect(this.#prev[index], this.#next[index]);
          }
          this.#connect(this.#tail, index);
          this.#tail = index;
        }
      }
      /**
       * Deletes a key out of the cache.
       *
       * Returns true if the key was deleted, false otherwise.
       */
      delete(k) {
        return this.#delete(k, "delete");
      }
      #delete(k, reason) {
        let deleted = false;
        if (this.#size !== 0) {
          const index = this.#keyMap.get(k);
          if (index !== void 0) {
            deleted = true;
            if (this.#size === 1) {
              this.#clear(reason);
            } else {
              this.#removeItemSize(index);
              const v = this.#valList[index];
              if (this.#isBackgroundFetch(v)) {
                v.__abortController.abort(new Error("deleted"));
              } else if (this.#hasDispose || this.#hasDisposeAfter) {
                if (this.#hasDispose) {
                  this.#dispose?.(v, k, reason);
                }
                if (this.#hasDisposeAfter) {
                  this.#disposed?.push([v, k, reason]);
                }
              }
              this.#keyMap.delete(k);
              this.#keyList[index] = void 0;
              this.#valList[index] = void 0;
              if (index === this.#tail) {
                this.#tail = this.#prev[index];
              } else if (index === this.#head) {
                this.#head = this.#next[index];
              } else {
                const pi = this.#prev[index];
                this.#next[pi] = this.#next[index];
                const ni = this.#next[index];
                this.#prev[ni] = this.#prev[index];
              }
              this.#size--;
              this.#free.push(index);
            }
          }
        }
        if (this.#hasDisposeAfter && this.#disposed?.length) {
          const dt = this.#disposed;
          let task;
          while (task = dt?.shift()) {
            this.#disposeAfter?.(...task);
          }
        }
        return deleted;
      }
      /**
       * Clear the cache entirely, throwing away all values.
       */
      clear() {
        return this.#clear("delete");
      }
      #clear(reason) {
        for (const index of this.#rindexes({ allowStale: true })) {
          const v = this.#valList[index];
          if (this.#isBackgroundFetch(v)) {
            v.__abortController.abort(new Error("deleted"));
          } else {
            const k = this.#keyList[index];
            if (this.#hasDispose) {
              this.#dispose?.(v, k, reason);
            }
            if (this.#hasDisposeAfter) {
              this.#disposed?.push([v, k, reason]);
            }
          }
        }
        this.#keyMap.clear();
        this.#valList.fill(void 0);
        this.#keyList.fill(void 0);
        if (this.#ttls && this.#starts) {
          this.#ttls.fill(0);
          this.#starts.fill(0);
        }
        if (this.#sizes) {
          this.#sizes.fill(0);
        }
        this.#head = 0;
        this.#tail = 0;
        this.#free.length = 0;
        this.#calculatedSize = 0;
        this.#size = 0;
        if (this.#hasDisposeAfter && this.#disposed) {
          const dt = this.#disposed;
          let task;
          while (task = dt?.shift()) {
            this.#disposeAfter?.(...task);
          }
        }
      }
    };
    exports2.LRUCache = LRUCache2;
  }
});

// main_database.js
var fs = require("fs");
var readline = require("readline");
var path = require("path");
var zlib = require("zlib");
var csv = require_csv_parser();
var crypto = require("crypto");
var util = require("util");
var gzipAsync = util.promisify(zlib.gzip);
var Database = require_lib();
var Queue = class {
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
    Promise.resolve().then(() => task()).catch((err) => {
      console.error("Queue task error:", err);
    }).finally(() => {
      this.running--;
      if (this.items.length > 0) {
        this._processNext();
      } else if (this.running === 0) {
        this._emit("end");
      }
    });
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
    for (let i = listeners.length - 1; i >= 0; i--) {
      const listener = listeners[i];
      listener.callback(...args);
      if (listener.once) {
        listeners.splice(i, 1);
      }
    }
  }
};
var rl = readline.createInterface({
  input: process.stdin,
  output: process.stdout
});
function log(level, message) {
  const timestamp = (/* @__PURE__ */ new Date()).toISOString();
  const logMessage = `${timestamp} [${level}] ${message}`;
  console.log(logMessage);
  if (level !== "DEBUG") {
    fs.appendFileSync("dbms.log", logMessage + "\n");
  }
}
function handleError(error, operation) {
  log("ERROR", `Error during ${operation}: ${error.message}`);
  console.error(`An error occurred during ${operation}. Please check the logs for more details.`);
}
var { LRUCache } = require_commonjs();
var PartitionedTable = class {
  constructor(tableName, partitionKey, numPartitions = 10) {
    this.tableName = tableName;
    this.partitionKey = partitionKey;
    this.numPartitions = numPartitions;
    this.partitions = Array.from({ length: numPartitions }, () => /* @__PURE__ */ new Map());
    this.compressed = false;
    this.compressionEnabled = false;
    this.riemannSphereDB = new RiemannSphereDB();
    this._verifyPartitions();
  }
  _verifyPartitions() {
    if (!Array.isArray(this.partitions) || this.partitions.length !== this.numPartitions) {
      throw new Error("Invalid partitions array");
    }
  }
  // 根据分区键计算分区索引
  getPartitionIndex(data) {
    const keyValue = String(data[this.partitionKey]);
    const hash = this.hash(keyValue);
    const partitionIndex = hash % this.numPartitions;
    return partitionIndex;
  }
  // 简单的哈希函数
  hash(key) {
    let hash = 0;
    for (let i = 0; i < key.length; i++) {
      hash = (hash << 5) - hash + key.charCodeAt(i);
      hash |= 0;
    }
    const absHash = Math.abs(hash);
    return absHash;
  }
  // 插入数据
  insert(data) {
    const partitionIndex = this.getPartitionIndex(data);
    const partition = this.partitions[partitionIndex];
    const key = String(data[this.partitionKey]);
    if (!partition) {
      this.partitions[partitionIndex] = /* @__PURE__ */ new Map();
    }
    this.partitions[partitionIndex].set(key, data);
  }
  // PartitionedTable 类中的 query 方法
  // PartitionedTable 类中的 query 方法
  query(query, fields = ["*"]) {
    const pk = this.partitionKey;
    if (query && query[pk] && typeof query[pk] === "object" && query[pk]._op === "=") {
      const key = String(query[pk]._value);
      const partitionIndex = this.getPartitionIndex({ [pk]: key });
      const partition = this.partitions[partitionIndex];
      if (partition && partition.has(key)) {
        const data = partition.get(key);
        if (fields && fields.length && fields[0] !== "*") {
          const filtered = {};
          fields.forEach((f) => {
            if (data.hasOwnProperty(f)) filtered[f] = data[f];
          });
          return [filtered];
        } else {
          return [{ ...data }];
        }
      }
      return [];
    }
    const results = [];
    this.partitions.forEach((partition) => {
      if (partition) {
        for (const [key, data] of partition.entries()) {
          if (data && typeof data === "object" && !Array.isArray(data)) {
            if (this.isMatch(data, query || {})) {
              if (fields && fields.length && fields[0] !== "*") {
                const filtered = {};
                fields.forEach((f) => {
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
      if (!query || Object.keys(query).length === 0) {
        return true;
      }
      if (query._op) {
        switch (query._op.toLowerCase()) {
          case "and":
            return query._conditions.every((condition) => this.isMatch(data, condition));
          case "or":
            return query._conditions.some((condition) => this.isMatch(data, condition));
          case "not":
            return !this.isMatch(data, query._condition);
          default:
            log("ERROR", `\u672A\u77E5\u7684\u903B\u8F91\u8FD0\u7B97\u7B26: ${query._op}`);
            return false;
        }
      }
      return Object.entries(query).every(([field, condition]) => {
        if (!data.hasOwnProperty(field)) {
          return false;
        }
        const fieldValue = data[field];
        if (condition.column && condition.subquery) {
          try {
            const subQueryResults = this._executeSubQuery(condition.subquery);
            if (!subQueryResults || subQueryResults.length === 0) {
              return false;
            }
            const targetValue = subQueryResults[0][condition.column];
            const numFieldValue = Number(fieldValue);
            const numTargetValue = Number(targetValue);
            if (!isNaN(numFieldValue) && !isNaN(numTargetValue)) {
              return numFieldValue === numTargetValue;
            }
            return String(fieldValue) === String(targetValue);
          } catch (error) {
            log("ERROR", `\u5B50\u67E5\u8BE2\u6267\u884C\u5931\u8D25: ${error.message}`);
            return false;
          }
        }
        if (typeof condition === "object" && condition._op) {
          const compareValue = condition._value;
          const numFieldValue = Number.isNaN(Number(fieldValue)) ? fieldValue : Number(fieldValue);
          const numCompareValue = Number.isNaN(Number(compareValue)) ? compareValue : Number(compareValue);
          switch (condition._op) {
            case "=":
              return numFieldValue === numCompareValue;
            case ">":
              return numFieldValue > numCompareValue;
            case "<":
              return numFieldValue < numCompareValue;
            case ">=":
              return numFieldValue >= numCompareValue;
            case "<=":
              return numFieldValue <= numCompareValue;
            case "<>":
              return numFieldValue !== numCompareValue;
            default:
              return false;
          }
        }
        return fieldValue === condition;
      });
    } catch (error) {
      log("ERROR", `\u5339\u914D\u68C0\u67E5\u5931\u8D25: ${error.message}`);
      return false;
    }
  }
  _executeSubQuery(subquery) {
    try {
      const tokens = subquery.trim().toLowerCase().split(/\s+/);
      const selectIndex = tokens.indexOf("select");
      const fromIndex = tokens.indexOf("from");
      const whereIndex = tokens.indexOf("where");
      if (selectIndex === -1 || fromIndex === -1 || selectIndex > fromIndex) {
        throw new Error("Invalid subquery format");
      }
      const targetColumns = tokens.slice(selectIndex + 1, fromIndex);
      if (targetColumns.length === 0) {
        throw new Error("No columns specified in subquery");
      }
      const tableName = tokens[fromIndex + 1];
      if (!tableName) {
        throw new Error("No table specified in subquery");
      }
      if (!global.hypercubeDB) {
        throw new Error("Database instance not found");
      }
      const currentDB = global.hypercubeDB.getCurrentDatabase();
      if (!currentDB) {
        throw new Error("No database selected");
      }
      const db2 = global.hypercubeDB.data.get(global.hypercubeDB.currentDatabase);
      if (!db2) {
        throw new Error("Current database not found");
      }
      const targetTable = db2.get(tableName);
      if (!targetTable) {
        throw new Error(`Table ${tableName} not found`);
      }
      let conditions = {};
      if (whereIndex !== -1) {
        const whereClause = tokens.slice(whereIndex + 1).join(" ");
        conditions = parseConditions(whereClause);
      }
      const results = targetTable.query(conditions).map((row) => {
        const selectedColumns = {};
        targetColumns.forEach((col) => {
          if (row.hasOwnProperty(col)) {
            selectedColumns[col] = row[col];
          }
        });
        return selectedColumns;
      }).filter((row) => Object.keys(row).length > 0);
      if (results.length === 0) {
        return [];
      }
      return results;
    } catch (error) {
      log("ERROR", `\u5B50\u67E5\u8BE2\u6267\u884C\u5931\u8D25: ${error.message}`);
      throw error;
    }
  }
  getRawKey(processedKey) {
    if (this.compressed) {
      return this.reverseKeyProcessing(processedKey);
    }
    return processedKey;
  }
  reverseKeyProcessing(key) {
    try {
      const crypto2 = require("crypto");
      return crypto2.createHash("sha256").update(String(key)).digest("hex");
    } catch (e) {
      return key;
    }
  }
};
var HypercubeDB = class {
  constructor({
    partitionFactor = 8,
    cacheStrategy = "dimensional",
    vectorizationThreshold = 50
  } = {}) {
    this.optimizationParams = {
      PARTITION_FACTOR: partitionFactor,
      CACHE_STRATEGY: cacheStrategy,
      VECTORIZATION_THRESHOLD: vectorizationThreshold
    };
    this.riemannSphereDB = new RiemannSphereDB();
    this.data = /* @__PURE__ */ new Map();
    this.indexes = /* @__PURE__ */ new Map();
    this.locks = /* @__PURE__ */ new Map();
    this.distanceHashes = /* @__PURE__ */ new Map();
    this.queryCache = /* @__PURE__ */ new Map();
    this.loadDataAsync();
    this.pyramidIndex = null;
    this.recursiveSphereWeaving = null;
    this._applyOptimizationConfig();
    this.isLoaded = false;
    this.extensions = /* @__PURE__ */ new Map();
    this.isDroppingDatabase = false;
    this.dimensiontree = null;
    this.mounts = /* @__PURE__ */ new Map();
    this.saveQueue = new Queue({ concurrency: 1, autostart: true });
    this.sqliteDBs = /* @__PURE__ */ new Map();
    this.pendingSaves = /* @__PURE__ */ new Set();
  }
  async mount(dbName, externalPath) {
    this.mounts.set(dbName, externalPath);
    await this._loadSingleDatabase(dbName, externalPath);
  }
  // 只加载单个数据库（支持本地和挂载路径）
  // ...existing code...
  // 只加载单个数据库（支持本地和挂载路径，自动识别SQLite/JSON）
  async _loadSingleDatabase(dbName, filePath) {
    try {
      const absPath = path.resolve(filePath);
      if (!fs.existsSync(absPath)) {
        log("ERROR", `Database file not found: ${absPath}`);
        return;
      }
      if (absPath.endsWith(".db")) {
        const sqliteDB = new Database(absPath);
        this.sqliteDBs.set(dbName, sqliteDB);
        const tables = sqliteDB.prepare("SELECT name FROM sqlite_master WHERE type='table'").all();
        const partitionedTables = /* @__PURE__ */ new Map();
        for (const { name: tableName } of tables) {
          const pageSize = 1e3;
          let offset = 0;
          let allRows = [];
          while (true) {
            const rows = sqliteDB.prepare(`SELECT * FROM ${tableName} LIMIT ? OFFSET ?`).all(pageSize, offset);
            if (rows.length === 0) break;
            allRows.push(...rows);
            offset += pageSize;
          }
          const partitionKey = "id";
          const numPartitions = 10;
          const partitionedTable = new PartitionedTable(tableName, partitionKey, numPartitions);
          for (const row of allRows) {
            partitionedTable.insert(row);
          }
          partitionedTables.set(tableName, partitionedTable);
        }
        this.data.set(dbName, partitionedTables);
        log("INFO", `Database ${dbName} loaded from SQLite file ${absPath}`);
      } else {
        const rawData = await fs.promises.readFile(absPath);
        const dbData = JSON.parse(rawData.toString());
        const partitionedTables = /* @__PURE__ */ new Map();
        for (const [tableName, tableData] of Object.entries(dbData)) {
          if (!tableData.partitionKey || !Array.isArray(tableData.partitions)) {
            log("ERROR", `Invalid table structure for ${tableName}`);
            continue;
          }
          const partitionedTable = new PartitionedTable(tableName, tableData.partitionKey, tableData.numPartitions || 10);
          partitionedTable.compressed = !!tableData.compressed;
          partitionedTable.partitions = tableData.partitions.map((partition) => {
            if (!Array.isArray(partition)) return /* @__PURE__ */ new Map();
            const validPartition = /* @__PURE__ */ new Map();
            for (const entry of partition) {
              if (!Array.isArray(entry) || entry.length !== 2) continue;
              const [key, value] = entry;
              if (key === void 0 || value === null) continue;
              try {
                if (partitionedTable.compressed && value?.compressed) {
                  const decompressed = this.conicalProjectionDecompress(value.compressed);
                  validPartition.set(key, decompressed);
                } else {
                  validPartition.set(key, value);
                }
              } catch (e) {
                log("ERROR", `Error processing ${key}: ${e.message}`);
                validPartition.set(key, value);
              }
            }
            return validPartition;
          });
          partitionedTables.set(tableName, partitionedTable);
        }
        this.data.set(dbName, partitionedTables);
        log("INFO", `Database ${dbName} loaded from ${absPath}`);
      }
    } catch (error) {
      log("ERROR", `Failed to load database ${dbName}: ${error.message}`);
    }
  }
  // 只保存单个数据库（支持本地和挂载路径）
  // ...existing code...
  // 只保存单个数据库（支持本地和挂载路径），异步队列+SQLite
  async _saveSingleDatabase(dbName, filePath) {
    try {
      if (!this.data.has(dbName)) {
        log("INFO", `No data to save for database: ${dbName}`);
        return;
      }
      let sqliteDB = this.sqliteDBs.get(dbName);
      if (!sqliteDB) {
        sqliteDB = new Database(path.resolve(filePath));
        this.sqliteDBs.set(dbName, sqliteDB);
      }
      sqliteDB.exec("BEGIN TRANSACTION");
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
          const pk = tableObj.partitionKey || "id";
          const columnsArr = Object.keys(sample).filter((col) => col !== pk);
          const columns = columnsArr.map((col) => `${col} TEXT`).join(",");
          sqliteDB.exec(`CREATE TABLE IF NOT EXISTS ${tableName} (${columns}, ${pk} TEXT PRIMARY KEY)`);
          sqliteDB.exec(`DELETE FROM ${tableName}`);
          const insertStmt = sqliteDB.prepare(
            `INSERT INTO ${tableName} (${[...columnsArr, pk].join(",")}) VALUES (${[...columnsArr.map(() => "?"), "?"].join(",")})`
          );
          let batch = [];
          for (const partition of tableObj.partitions) {
            if (!partition) continue;
            for (const [id, row] of partition.entries()) {
              batch.push([...columnsArr.map((col) => row[col]), id]);
              if (batch.length >= 1e3) {
                for (const params of batch) insertStmt.run(...params);
                batch = [];
              }
            }
          }
          for (const params of batch) insertStmt.run(...params);
        }
        sqliteDB.exec("COMMIT");
        log("INFO", `Database ${dbName} saved to SQLite file ${filePath}`);
      } catch (txError) {
        sqliteDB.exec("ROLLBACK");
        log("ERROR", `Transaction failed for ${dbName}: ${txError.message}`);
      }
    } catch (error) {
      log("ERROR", `Save operation failed for ${dbName}: ${error.message}`);
    }
  }
  async demount(dbName) {
    if (!this.mounts.has(dbName)) return;
    const externalPath = this.mounts.get(dbName);
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
    this.cache = this.optimizationParams.CACHE_STRATEGY === "dimensional" ? new LRUCache({ max: 5e3 }) : null;
  }
  // 有损压缩方法（集成黎曼球面映射）
  conicalProjectionCompress(dataPoint, focalPoint = [0, 0, -Infinity]) {
    if (!dataPoint || typeof dataPoint !== "object") {
      log("ERROR", "Invalid data point for compression");
      return [Date.now() % 1e3, 0, 0];
    }
    const mappedPoint = this.riemannSphereDB.mapToRiemannSphere(dataPoint);
    if (this.compressionDisabled) {
      return dataPoint;
    }
    if (!dataPoint || typeof dataPoint !== "object") {
      log("ERROR", "Invalid data point for compression");
      return [Date.now() % 1e3, 0, 0];
    }
    let dpArray;
    try {
      if (!crypto || !crypto.createHash) {
        throw new Error("Crypto module not available");
      }
      if (Array.isArray(dataPoint)) {
        dpArray = dataPoint.filter((v) => v !== void 0 && v !== null);
      } else {
        const values = Object.values(dataPoint);
        dpArray = values.length > 0 ? values : [dataPoint];
      }
      if (dpArray.length === 0 || dpArray.some((v) => v === void 0)) {
        return [Date.now() % 1e3, 0, 0];
      }
      dpArray = dpArray.map((v) => {
        if (typeof v === "string") {
          return parseInt(crypto.createHash("md5").update(v).digest("hex"), 16) % 1e3;
        }
        const num = Number(v);
        return isNaN(num) ? 0 : num;
      });
      while (dpArray.length < 3) {
        dpArray.push(0);
      }
      const spherePoint = this.riemannSphereDB.mapToRiemannSphere(dpArray);
      if (!spherePoint || spherePoint.some(isNaN)) {
        log("ERROR", "Invalid sphere mapping result");
        return [Date.now() % 1e3, 0, 0];
      }
      const toPoint = spherePoint.map((v, i) => v - this.sphereCenter[i]);
      const baseAxis = focalPoint.map((v, i) => v - this.sphereCenter[i]);
      const theta = this.calculateAngle(baseAxis, toPoint);
      const phi = Math.atan2(toPoint[1], toPoint[0]);
      return [
        Math.round(Math.tan(theta) * 1e4) / 1e4,
        Math.round(phi * 180 / Math.PI),
        this.calculateIntensity(spherePoint),
        Date.now() % 1e3
      ];
    } catch (error) {
      log("ERROR", `Compression failed: ${error.message}`);
      return [Date.now() % 1e3, 0, 0];
    }
  }
  // 解压缩方法
  conicalProjectionDecompress(compressedData) {
    const theta = Math.atan(compressedData[0]);
    const phi = compressedData[1] * Math.PI / 180;
    const intensity = compressedData[2];
    const x = intensity * Math.sin(theta) * Math.cos(phi);
    const y = intensity * Math.sin(theta) * Math.sin(phi);
    const z = intensity * Math.cos(theta);
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
      delete require.cache[fullPath];
      const extension = require(fullPath);
      const extName = path.basename(extensionPath, ".js").toLowerCase();
      if (extension && extension.execute && typeof extension.execute === "object") {
        this.extensions.set(extName, extension);
        log("INFO", `Extension '${extName}' registered`);
        console.log(`\u6210\u529F\u52A0\u8F7D\u6269\u5C55: ${extName}`);
      } else {
        console.log(`Invalid extension format: ${extensionPath}`);
        console.log(`Extension should have an 'execute' object with methods`);
      }
    } catch (error) {
      console.log(`Failed to load extension: ${error.message}`);
      handleError(error, "loading extension");
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
      console.log("Loaded extensions:");
      this.extensions.forEach((extension, name) => {
        const methods = extension.execute ? Object.keys(extension.execute) : [];
        console.log(`- ${name}: [${methods.join(", ")}]`);
      });
    } else {
      console.log("No extensions found");
    }
  }
  executeExtension(extensionName, method, ...args) {
    const extKey = extensionName.toLowerCase();
    if (!this.extensions.has(extKey)) {
      console.log(`Extension '${extensionName}' not found`);
      log("ERROR", `Extension '${extensionName}' not found`);
      this.showExtensions();
      return;
    }
    const extension = this.extensions.get(extKey);
    const methodKey = method ? method.toLowerCase() : null;
    if (!methodKey || !extension.execute || typeof extension.execute[methodKey] !== "function") {
      console.log(`Method '${method}' not found in extension '${extensionName}'`);
      log("ERROR", `Method '${method}' not found in extension '${extensionName}'`);
      return;
    }
    try {
      const result = extension.execute[methodKey](this, ...args);
      log("INFO", `Extension '${extensionName}' executed successfully`);
      return result;
    } catch (error) {
      console.log(`Error executing extension '${extensionName}':`, error.message);
      log("ERROR", `Error executing extension '${extensionName}': ${error.message}`);
    }
  }
  showDatabases() {
    try {
      const databases = Array.from(this.data.keys());
      if (databases.length === 0) {
        console.log("No databases found");
      } else {
        console.log("Databases:");
        databases.forEach((db2) => console.log(db2));
      }
    } catch (error) {
      handleError(error, "listing databases");
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
        tables.forEach((table) => console.log(table));
      }
      log("INFO", "Tables listed successfully");
    } catch (error) {
      handleError(error, "listing tables");
    }
  }
  initializeCentroids(dataPoints, k) {
    const indices = Array.from({ length: dataPoints.length }, (_, i) => i);
    const randomIndices = indices.sort(() => Math.random() - 0.5).slice(0, k);
    return randomIndices.map((index) => dataPoints[index]);
  }
  assignClusters(dataPoints, centroids) {
    const clusters = Array.from({ length: centroids.length }, () => []);
    dataPoints.forEach((dataPoint, index) => {
      const distances = centroids.map((centroid) => this.euclideanDistance(dataPoint, centroid));
      const closestCentroidIndex = distances.indexOf(Math.min(...distances));
      clusters[closestCentroidIndex].push(index);
    });
    return clusters;
  }
  calculateCentroids(clusters, dataPoints) {
    return clusters.map((cluster) => {
      if (cluster.length === 0) {
        return dataPoints[Math.floor(Math.random() * dataPoints.length)];
      }
      const sum = cluster.reduce((acc, index) => {
        return acc.map((sum2, i) => sum2 + dataPoints[index][i]);
      }, Array(dataPoints[0].length).fill(0));
      return sum.map((sum2) => sum2 / cluster.length);
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
        if (!(tableData instanceof PartitionedTable)) {
          console.log("Invalid table data structure");
          this.releaseLock(tableName);
          return;
        }
        dataArray.forEach((data) => {
          const validData = {};
          Object.keys(data).forEach((key) => {
            if (typeof key === "string" && key.trim() !== "") {
              validData[key.trim()] = data[key];
            }
          });
          tableData.insert(validData);
          Object.keys(validData).forEach((dim) => {
            if (this.indexes.has(tableName) && this.indexes.get(tableName).has(dim)) {
              const indexValue = validData[dim];
              if (!this.indexes.get(tableName).get(dim).has(indexValue)) {
                this.indexes.get(tableName).get(dim).set(indexValue, []);
              }
              this.indexes.get(tableName).get(dim).get(indexValue).push(validData);
            }
          });
          if (this.indexes.has(tableName)) {
            for (const [indexKey, indexObj] of this.indexes.get(tableName).entries()) {
              if (indexObj instanceof DimensionsTree) {
                const dims = indexObj.dimensions;
                const keys = dims.map((dim) => data[dim]);
                indexObj.insert(keys, data);
              }
            }
          }
          if (this.pyramidIndex && Array.isArray(this.pyramidIndex.data)) {
            this.pyramidIndex.data.push(data);
            if (typeof this.pyramidIndex.insert === "function") {
              this.pyramidIndex.insert(data);
            }
          }
          if (this.recursiveSphereWeaving && typeof this.recursiveSphereWeaving.insert === "function") {
            this.recursiveSphereWeaving.insert(data);
          }
        });
        this.saveData();
        this.releaseLock(tableName);
        console.log(`Inserted ${dataArray.length} records`);
      } else {
        console.log("Table is locked");
      }
    } catch (error) {
      handleError(error, "batch insert");
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
            Object.assign(oldData, newData);
            Object.keys(newData).forEach((dim) => {
              if (this.indexes.has(tableName) && this.indexes.get(tableName).has(dim)) {
                const indexValue = newData[dim];
                if (!this.indexes.get(tableName).get(dim).has(indexValue)) {
                  this.indexes.get(tableName).get(dim).set(indexValue, []);
                }
                this.indexes.get(tableName).get(dim).get(indexValue).push(oldData);
              }
            });
            if (this.indexes.has(tableName)) {
              for (const [indexKey, indexObj] of this.indexes.get(tableName).entries()) {
                if (indexObj instanceof DimensionsTree) {
                  const dims = indexObj.dimensions;
                  const keys = dims.map((dim) => oldData[dim]);
                  indexObj.update(keys, oldData, { ...oldData });
                }
              }
            }
            if (this.pyramidIndex && typeof this.pyramidIndex.update === "function") {
              this.pyramidIndex.update(oldData, { ...oldData });
            }
            if (this.recursiveSphereWeaving && typeof this.recursiveSphereWeaving.update === "function") {
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
      handleError(error, "batch update");
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
      partitionedTable.partitions.forEach((partition) => {
        if (partition) {
          for (let [key, record] of partition.entries()) {
            if (partitionedTable.isMatch(record, condition)) {
              Object.assign(record, newData);
              Object.keys(newData).forEach((dim) => {
                if (this.indexes.has(tableName) && this.indexes.get(tableName).has(dim)) {
                  const indexValue = newData[dim];
                  if (!this.indexes.get(tableName).get(dim).has(indexValue)) {
                    this.indexes.get(tableName).get(dim).set(indexValue, []);
                  }
                  this.indexes.get(tableName).get(dim).get(indexValue).push(record);
                }
              });
              if (this.indexes.has(tableName)) {
                for (const [indexKey, indexObj] of this.indexes.get(tableName).entries()) {
                  if (indexObj instanceof DimensionsTree) {
                    const dims = indexObj.dimensions;
                    const keys = dims.map((dim) => record[dim]);
                    indexObj.update(keys, record, { ...record, ...newData });
                  }
                }
              }
              if (this.pyramidIndex && typeof this.pyramidIndex.update === "function") {
                this.pyramidIndex.update(record, { ...record, ...newData });
              }
              if (this.recursiveSphereWeaving && typeof this.recursiveSphereWeaving.update === "function") {
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
      handleError(error, "batch column update");
    }
  }
  // 修改后的 loadDataAsync 方法 - 使用SQLite加载
  async loadDataAsync() {
    try {
      const dbFiles = await fs.promises.readdir(process.cwd());
      for (const file of dbFiles) {
        if (!file.endsWith(".db")) continue;
        const dbName = path.basename(file, ".db");
        const sqliteDB = new Database(path.join(process.cwd(), file));
        this.sqliteDBs.set(dbName, sqliteDB);
        const tables = sqliteDB.prepare("SELECT name FROM sqlite_master WHERE type='table'").all();
        const partitionedTables = /* @__PURE__ */ new Map();
        for (const { name: tableName } of tables) {
          const pageSize = 1e3;
          let offset = 0;
          let allRows = [];
          while (true) {
            const rows = sqliteDB.prepare(`SELECT * FROM ${tableName} LIMIT ? OFFSET ?`).all(pageSize, offset);
            if (rows.length === 0) break;
            allRows.push(...rows);
            offset += pageSize;
          }
          const partitionKey = "id";
          const numPartitions = 10;
          const partitionedTable = new PartitionedTable(tableName, partitionKey, numPartitions);
          for (const row of allRows) {
            partitionedTable.insert(row);
          }
          partitionedTables.set(tableName, partitionedTable);
        }
        this.data.set(dbName, partitionedTables);
      }
      log("INFO", "All databases loaded from SQLite");
    } catch (error) {
      log("ERROR", `Failed to load databases: ${error.message}`);
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
        sqliteDB.exec("BEGIN TRANSACTION");
        try {
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
            const pk = tableObj.partitionKey || "id";
            const columnsArr = Object.keys(sample).filter((col) => col !== pk);
            const columns = columnsArr.map((col) => `${col} TEXT`).join(",");
            sqliteDB.exec(`CREATE TABLE IF NOT EXISTS ${tableName} (${columns}, ${pk} TEXT PRIMARY KEY)`);
            sqliteDB.exec(`DELETE FROM ${tableName}`);
            const insertStmt = sqliteDB.prepare(
              `INSERT INTO ${tableName} (${[...columnsArr, pk].join(",")}) VALUES (${[...columnsArr.map(() => "?"), "?"].join(",")})`
            );
            let batch = [];
            for (const partition of tableObj.partitions) {
              if (!partition) continue;
              for (const [id, row] of partition.entries()) {
                batch.push([...columnsArr.map((col) => row[col]), id]);
                if (batch.length >= 1e3) {
                  for (const params of batch) insertStmt.run(...params);
                  batch = [];
                }
              }
            }
            for (const params of batch) insertStmt.run(...params);
          }
          sqliteDB.exec("COMMIT");
        } catch (txError) {
          sqliteDB.exec("ROLLBACK");
          log("ERROR", `Transaction failed for ${dbName}: ${txError.message}`);
        }
      }
      log("INFO", "All databases saved to SQLite");
    } catch (error) {
      log("ERROR", `Save operation failed: ${error.message}`);
    }
  }
  // 新增方法：处理实际的保存操作
  async _processSave(dbName) {
    if (!this.data.has(dbName)) {
      log("INFO", `No data to save for database: ${dbName}`);
      this.pendingSaves.delete(dbName);
      return;
    }
    try {
      if (!this.sqliteDBs.has(dbName)) {
        const dbPath = path.resolve(process.cwd(), `${dbName}.db`);
        this.sqliteDBs.set(dbName, new Database(dbPath, { verbose: log }));
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
      sqliteDB.exec("BEGIN TRANSACTION");
      try {
        for (const [tableName, table] of dbData.entries()) {
          if (!(table instanceof PartitionedTable)) continue;
          const tableExists = sqliteDB.prepare(`SELECT name FROM sqlite_master WHERE type='table' AND name='${tableName}_data'`).get();
          if (!tableExists) {
            sqliteDB.exec(`
                        CREATE TABLE ${tableName}_data (
                            id TEXT PRIMARY KEY,
                            partition_index INTEGER,
                            data TEXT
                        );
                        CREATE INDEX idx_${tableName}_partition ON ${tableName}_data(partition_index);
                    `);
            const configStmt = sqliteDB.prepare("INSERT OR REPLACE INTO _dbms_meta VALUES (?, ?, ?)");
            configStmt.run(tableName, "config", JSON.stringify({
              partitionKey: table.partitionKey,
              numPartitions: table.numPartitions,
              compressed: table.compressed
            }));
          }
          const insertStmt = sqliteDB.prepare(`INSERT OR REPLACE INTO ${tableName}_data VALUES (?, ?, ?)`);
          for (let partitionIndex = 0; partitionIndex < table.partitions.length; partitionIndex++) {
            const partition = table.partitions[partitionIndex];
            if (!partition) continue;
            for (const [key, value] of partition.entries()) {
              let dataToStore;
              if (table.compressed && value) {
                const compressed = Array.isArray(value) ? { compressed: this.conicalProjectionCompress(value) } : value;
                dataToStore = JSON.stringify(compressed);
              } else {
                dataToStore = JSON.stringify(value);
              }
              insertStmt.run(key, partitionIndex, dataToStore);
            }
          }
        }
        sqliteDB.exec("COMMIT");
        log("INFO", `Database ${dbName} saved to SQLite backend successfully`);
      } catch (txError) {
        sqliteDB.exec("ROLLBACK");
        log("ERROR", `Transaction failed for ${dbName}: ${txError.message}`);
      }
    } catch (error) {
      log("ERROR", `Save operation failed for ${dbName}: ${error.message}`);
    } finally {
      this.pendingSaves.delete(dbName);
    }
  }
  // 新增方法：启动保存队列处理器
  _startSaveQueueProcessor() {
    setInterval(async () => {
      if (this.pendingSaves.size > 0 && this.saveQueue.length < 5) {
        for (const dbName of this.pendingSaves) {
          this.saveQueue.push(async () => {
            await this._processSave(dbName);
          });
        }
      }
    }, 1e3);
  }
  // 修改关闭方法，确保所有数据保存完毕
  async close() {
    await new Promise((resolve) => {
      if (this.saveQueue.length === 0) {
        resolve();
        return;
      }
      this.saveQueue.once("end", resolve);
    });
    for (const [dbName, sqliteDB] of this.sqliteDBs.entries()) {
      sqliteDB.close();
    }
    log("INFO", "Database system shutdown successfully");
  }
  // ...existing code...
  acquireLock(tableName) {
    if (!this.locks.has(tableName)) {
      this.locks.set(tableName, true);
      return true;
    }
    return false;
  }
  releaseLock(tableName) {
    this.locks.delete(tableName);
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
        const indexKey = dimensions.join("|");
        if (!this.indexes.has(tableName)) {
          this.indexes.set(tableName, /* @__PURE__ */ new Map());
        }
        if (!this.indexes.get(tableName).has(indexKey)) {
          const dimensionTree = new DimensionsTree(dimensions);
          this.indexes.get(tableName).set(indexKey, dimensionTree);
          console.log(`Composite index on ${dimensions.join(", ")} added`);
          this.dimensiontree = dimensionTree;
        } else {
          console.log("Composite index already exists for these dimensions");
        }
        this.releaseLock(tableName);
        log("INFO", "Composite index added successfully");
      } else {
        console.log("Table is locked");
      }
    } catch (error) {
      handleError(error, "adding composite index");
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
      const indexMap = this.indexes.get(tableName);
      const queryDims = Object.keys(dims);
      console.log("\u5DF2\u5B58\u5728\u7684\u7D22\u5F15key:", Array.from(indexMap.keys()));
      console.log("\u672C\u6B21\u67E5\u8BE2\u7684\u7EF4\u5EA6:", queryDims);
      let foundKey = null;
      for (const key of indexMap.keys()) {
        const keyDims2 = key.split("|");
        if (keyDims2.length === queryDims.length && keyDims2.every((d) => queryDims.includes(d)) && queryDims.every((d) => keyDims2.includes(d))) {
          foundKey = key;
          break;
        }
      }
      if (!foundKey) {
        console.log("Composite index does not exist for these dimensions");
        return;
      }
      const dimensionTree = indexMap.get(foundKey);
      const keyDims = foundKey.split("|");
      const range = keyDims.map((dim) => {
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
        this.data.set(name, /* @__PURE__ */ new Map());
        this.saveData();
        console.log("Database created");
      }
      log("INFO", "Database created successfully");
    } catch (error) {
      handleError(error, "creating database");
    }
  }
  createTable(tableName, schema, partitionKey = "id", numPartitions = 10) {
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
            this.data.set(this.currentDatabase, /* @__PURE__ */ new Map());
          }
          const partitionedTable = new PartitionedTable(tableName, partitionKey, numPartitions);
          this.data.get(this.currentDatabase).set(tableName, partitionedTable);
          this.releaseLock(tableName);
          console.log(`Table created with partitioning by ${partitionKey}, partitions: ${numPartitions}`);
        } else {
          console.log("Table is locked");
        }
      }
      log("INFO", "Table created successfully");
    } catch (error) {
      handleError(error, "creating table");
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
      log("INFO", "Database selected successfully");
    } catch (error) {
      handleError(error, "using database");
    }
  }
  // 修改后的 createTable 方法
  /*createTable(tableName, schema, partitionKey = 'id') {
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
  */
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
        if (!this.indexes.has(tableName)) {
          this.indexes.set(tableName, /* @__PURE__ */ new Map());
        }
        if (!this.indexes.get(tableName).has(dimension)) {
          this.indexes.get(tableName).set(dimension, /* @__PURE__ */ new Map());
          const partitionedTable = this.data.get(this.currentDatabase).get(tableName);
          partitionedTable.partitions.forEach((partition) => {
            if (partition) {
              for (const data of partition.values()) {
                if (data[dimension] === void 0) {
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
      handleError(error, "adding dimension");
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
        const partitionedTable = this.data.get(this.currentDatabase).get(tableName);
        partitionedTable.partitions.forEach((partition) => {
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
      handleError(error, "Removing dimension");
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
        console.log("Inserting data:", data);
        if (typeof data !== "object" || data === null) {
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
        Object.keys(data).forEach((key) => {
          if (typeof key === "string" && key.trim() !== "" && data[key] !== 0) {
            validData[key.trim()] = data[key];
          } else if (typeof key === "string" && key.trim() !== "") {
            console.log(`Ignoring field ${key} with value 0`);
          }
        });
        data = validData;
        partitionedTable.insert(data);
        if (this.indexes.has(tableName)) {
          for (const [indexKey, indexObj] of this.indexes.get(tableName).entries()) {
            if (indexObj instanceof DimensionsTree) {
              const dims = indexObj.dimensions;
              const keys = dims.map((dim) => data[dim]);
              indexObj.insert(keys, data);
            }
          }
        }
        if (this.pyramidIndex && Array.isArray(this.pyramidIndex.data)) {
          this.pyramidIndex.data.push(data);
          if (typeof this.pyramidIndex.insert === "function") {
            this.pyramidIndex.insert(data);
          }
        }
        if (this.recursiveSphereWeaving && typeof this.recursiveSphereWeaving.insert === "function") {
          this.recursiveSphereWeaving.insert(data);
        }
        Object.keys(data).forEach((dim) => {
          if (!this.indexes.has(tableName)) {
            this.indexes.set(tableName, /* @__PURE__ */ new Map());
          }
          if (!this.indexes.get(tableName).has(dim)) {
            this.indexes.get(tableName).set(dim, /* @__PURE__ */ new Map());
          }
          const indexValue = data[dim];
          if (!this.indexes.get(tableName).get(dim).has(indexValue)) {
            this.indexes.get(tableName).get(dim).set(indexValue, []);
          }
          this.indexes.get(tableName).get(dim).get(indexValue).push(data);
        });
        this.saveData();
        this.releaseLock(tableName);
        console.log("Data inserted");
        log("INFO", "Data inserted successfully");
      } else {
        console.log("Table is locked");
      }
    } catch (error) {
      handleError(error, "inserting data");
    }
  }
  insertIntoDistanceHash(tableName, data) {
    if (!this.distanceHashes.has(tableName)) {
      this.distanceHashes.set(tableName, /* @__PURE__ */ new Map());
    }
    const dimensions = Object.keys(data);
    const numDimensions = dimensions.length;
    const numBuckets = 10;
    const bucketSize = 0.1;
    for (let i = 0; i < numBuckets; i++) {
      const bucketKey = i * bucketSize;
      if (!this.distanceHashes.get(tableName).has(bucketKey)) {
        this.distanceHashes.get(tableName).set(bucketKey, []);
      }
      const distance = Math.sqrt(dimensions.reduce((sum, dim) => sum + Math.pow(data[dim], 2), 0));
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
          this.data.get(this.currentDatabase).delete(tableName);
          this.indexes.delete(tableName);
          this.locks.delete(tableName);
          this.distanceHashes.delete(tableName);
          this.queryCache.forEach((value, key) => {
            if (key.tableName === tableName) {
              this.queryCache.delete(key);
            }
          });
          this.saveData();
          this.releaseLock(tableName);
          console.log("Table dropped");
        } else {
          console.log("Table does not exist");
        }
        log("INFO", "Table dropped successfully");
      } else {
        console.log("Table is locked");
      }
    } catch (error) {
      handleError(error, "dropping table");
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
      const db2 = this.data.get(this.currentDatabase);
      if (!db2 || !db2.has(tableName)) return 0;
      if (!this.acquireLock(tableName)) return 0;
      const partitionedTable = db2.get(tableName);
      if (!(partitionedTable instanceof PartitionedTable)) {
        this.releaseLock(tableName);
        return 0;
      }
      const pk = partitionedTable.partitionKey;
      let deletedCount = 0;
      if (condition && condition[pk] && typeof condition[pk] === "object" && condition[pk]._op === "=") {
        const key = String(condition[pk]._value);
        const partitionIndex = partitionedTable.getPartitionIndex({ [pk]: key });
        const partition = partitionedTable.partitions[partitionIndex];
        if (partition && partition.has(key)) {
          const record = partition.get(key);
          Object.keys(record).forEach((dim) => {
            if (this.indexes.get(tableName)?.has(dim)) {
              const dimMap = this.indexes.get(tableName).get(dim);
              const value = record[dim];
              if (dimMap.has(value)) {
                dimMap.set(value, dimMap.get(value).filter((item) => item[pk] !== key));
              }
            }
          });
          if (this.indexes.has(tableName)) {
            for (const [indexKey, indexObj] of this.indexes.get(tableName).entries()) {
              if (indexObj instanceof DimensionsTree) {
                const dims = indexObj.dimensions;
                const keys = dims.map((dim) => record[dim]);
                indexObj.delete(keys, record);
              }
            }
          }
          if (this.pyramidIndex && typeof this.pyramidIndex.delete === "function") {
            this.pyramidIndex.delete(record);
          }
          if (this.recursiveSphereWeaving && typeof this.recursiveSphereWeaving.delete === "function") {
            this.recursiveSphereWeaving.delete(record);
          }
          partition.delete(key);
          deletedCount = 1;
        }
      } else {
        partitionedTable.partitions.forEach((partition) => {
          if (partition) {
            for (let [key, record] of Array.from(partition.entries())) {
              if (partitionedTable.isMatch(record, condition)) {
                Object.keys(record).forEach((dim) => {
                  if (this.indexes.get(tableName)?.has(dim)) {
                    const dimMap = this.indexes.get(tableName).get(dim);
                    const value = record[dim];
                    if (dimMap.has(value)) {
                      dimMap.set(value, dimMap.get(value).filter((item) => item[pk] !== key));
                    }
                  }
                });
                if (this.indexes.has(tableName)) {
                  for (const [indexKey, indexObj] of this.indexes.get(tableName).entries()) {
                    if (indexObj instanceof DimensionsTree) {
                      const dims = indexObj.dimensions;
                      const keys = dims.map((dim) => record[dim]);
                      indexObj.delete(keys, record);
                    }
                  }
                }
                if (this.pyramidIndex && typeof this.pyramidIndex.delete === "function") {
                  this.pyramidIndex.delete(record);
                }
                if (this.recursiveSphereWeaving && typeof this.recursiveSphereWeaving.delete === "function") {
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
      if (condition && condition[pk] && typeof condition[pk] === "object" && condition[pk]._op === "=") {
        const key = String(condition[pk]._value);
        const partitionIndex = partitionedTable.getPartitionIndex({ [pk]: key });
        const partition = partitionedTable.partitions[partitionIndex];
        if (partition && partition.has(key)) {
          const record = partition.get(key);
          Object.assign(record, newData);
          Object.keys(newData).forEach((dim) => {
            if (this.indexes.has(tableName) && this.indexes.get(tableName).has(dim)) {
              const indexValue = newData[dim];
              if (!this.indexes.get(tableName).get(dim).has(indexValue)) {
                this.indexes.get(tableName).get(dim).set(indexValue, []);
              }
              this.indexes.get(tableName).get(dim).get(indexValue).push(record);
            }
          });
          if (this.indexes.has(tableName)) {
            for (const [indexKey, indexObj] of this.indexes.get(tableName).entries()) {
              if (indexObj instanceof DimensionsTree) {
                const dims = indexObj.dimensions;
                const keys = dims.map((dim) => record[dim]);
                indexObj.update(keys, record, { ...record, ...newData });
              }
            }
          }
          if (this.pyramidIndex && typeof this.pyramidIndex.update === "function") {
            this.pyramidIndex.update(record, { ...record, ...newData });
          }
          if (this.recursiveSphereWeaving && typeof this.recursiveSphereWeaving.update === "function") {
            this.recursiveSphereWeaving.update(record, { ...record, ...newData });
          }
          updatedCount = 1;
        }
      } else {
        partitionedTable.partitions.forEach((partition) => {
          if (partition) {
            for (let [key, record] of partition.entries()) {
              if (partitionedTable.isMatch(record, condition)) {
                Object.assign(record, newData);
                Object.keys(newData).forEach((dim) => {
                  if (this.indexes.has(tableName) && this.indexes.get(tableName).has(dim)) {
                    const indexValue = newData[dim];
                    if (!this.indexes.get(tableName).get(dim).has(indexValue)) {
                      this.indexes.get(tableName).get(dim).set(indexValue, []);
                    }
                    this.indexes.get(tableName).get(dim).get(indexValue).push(record);
                  }
                });
                if (this.indexes.has(tableName)) {
                  for (const [indexKey, indexObj] of this.indexes.get(tableName).entries()) {
                    if (indexObj instanceof DimensionsTree) {
                      const dims = indexObj.dimensions;
                      const keys = dims.map((dim) => record[dim]);
                      indexObj.update(keys, record, { ...record, ...newData });
                    }
                  }
                }
                if (this.pyramidIndex && typeof this.pyramidIndex.update === "function") {
                  this.pyramidIndex.update(record, { ...record, ...newData });
                }
                if (this.recursiveSphereWeaving && typeof this.recursiveSphereWeaving.update === "function") {
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
  selectData(tableName, query, fields = ["*"]) {
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
          this.queryCache.delete(cacheKey);
          const partitionedTable = currentDB.get(tableName);
          if (partitionedTable && partitionedTable instanceof PartitionedTable) {
            let results = partitionedTable.query(query || {}, fields);
            if (query && query.distance !== void 0) {
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
                  const bucketResults = this.distanceHashes.get(tableName).get(bucket).filter((data) => {
                    const pointDistance = Math.sqrt(
                      Object.keys(data).reduce((sum, dim) => sum + Math.pow(data[dim], 2), 0)
                    );
                    return pointDistance >= distance - bucketSize && pointDistance < distance + bucketSize;
                  }).map((data) => {
                    if (fields && fields.length && fields[0] !== "*") {
                      const filtered = {};
                      fields.forEach((f) => {
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
              const seen = /* @__PURE__ */ new Set();
              results = results.filter((data) => {
                const key = JSON.stringify(data);
                return seen.has(key) ? false : seen.add(key);
              });
            } else {
              if (query && query.label !== void 0) {
                results = results.filter((data) => data.label === query.label);
              }
              const seen = /* @__PURE__ */ new Set();
              results = results.filter((data) => {
                const key = JSON.stringify(data);
                return seen.has(key) ? false : seen.add(key);
              });
              results = results.filter((data) => data !== null);
              results = results.filter((data) => data !== void 0);
              results = results.map((data) => {
                const filteredData = {};
                Object.keys(data).forEach((key) => {
                  if (data[key] !== void 0) {
                    filteredData[key] = data[key];
                  }
                });
                return filteredData;
              });
            }
            if (query && query.label !== void 0) {
              results = results.filter((data) => data.label === query.label);
            }
            console.log(results);
            this.queryCache.set(cacheKey, results);
            return { data: results };
          } else {
            console.log("partitionedTable is not an instance of PartitionedTable or is undefined");
          }
        } catch (error) {
          handleError(error, "selecting data");
          return { data: [] };
        } finally {
          this.releaseLock(tableName);
        }
      } else {
        console.log("Table is locked");
      }
      log("INFO", "Data selected successfully");
    } catch (error) {
      handleError(error, "selecting data");
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
        partitionedTable.partitions.forEach((partition) => {
          if (partition) {
            for (let [key, data] of partition.entries()) {
              if (partitionedTable.isMatch(data, query)) {
                if (data.hasOwnProperty(column)) {
                  delete data[column];
                  updated = true;
                }
              }
            }
          }
        });
        if (updated) {
          this.updateIndexesAfterColumnDeletion(tableName, column);
          this.saveData();
          console.log(`Column ${column} deleted from records matching the condition`);
        } else {
          console.log("No records found matching the condition");
        }
        this.releaseLock(tableName);
        log("INFO", "Column deleted successfully");
      } else {
        console.log("Table is locked");
      }
    } catch (error) {
      handleError(error, "deleting column");
      this.releaseLock(tableName);
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
    if (!this.data.has(this.currentDatabase) || !this.data.get(this.currentDatabase).has(tableName)) {
      console.log("Table does not exist");
      return;
    }
    const partitionedTable = this.data.get(this.currentDatabase).get(tableName);
    let dataPoints = [];
    partitionedTable.partitions.forEach((partition) => {
      if (partition) {
        for (const data of partition.values()) {
          dataPoints.push(data);
        }
      }
    });
    const filteredData = dataPoints.filter(
      (data) => data[dim1] !== void 0 && data[dim2] !== void 0
    );
    return (x, y) => {
      let sum = 0;
      let count = 0;
      filteredData.forEach((data) => {
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
  importFromCSV(tableName, filePath) {
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
      fs.createReadStream(filePath).pipe(csv()).on("data", (row) => {
        try {
          const cleanedRow = {};
          Object.keys(row).forEach((key) => {
            const trimmedKey = key.trim();
            if (trimmedKey) {
              let value = row[key];
              if (!isNaN(value) && value !== "") {
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
      }).on("end", () => {
        console.log(`CSV file successfully imported. ${rowCount} rows processed.`);
        this.releaseLock(tableName);
        log("INFO", `CSV import completed: ${rowCount} rows`);
      }).on("error", (error) => {
        console.log("CSV import error:", error.message);
        this.releaseLock(tableName);
        handleError(error, "CSV import");
      });
    } catch (error) {
      this.releaseLock(tableName);
      handleError(error, "CSV import setup");
    }
  }
  // 高维球面映射算法
  projectToRiemannSphere(tableName, dimensions, viewpoint = [0, 0, 0]) {
    if (!this.currentDatabase || !this.data.has(this.currentDatabase) || !this.data.get(this.currentDatabase).has(tableName)) {
      console.log("Database/Table not selected");
      return [];
    }
    const partitionedTable = this.data.get(this.currentDatabase).get(tableName);
    let points = [];
    partitionedTable.partitions.forEach((partition) => {
      if (partition) {
        for (const data of partition.values()) {
          points.push(data);
        }
      }
    });
    return points.map((data) => {
      const rawCoords = dimensions.map((d) => parseFloat(data[d]) || 0);
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
    return {
      x: Math.sin(theta) * Math.cos(phi),
      y: Math.sin(theta) * Math.sin(phi),
      z: Math.cos(theta)
    };
  }
  // 光线投射查询
  raycastQuery(tableName, origin, direction) {
    const points = this.projectToRiemannSphere(tableName, ["x", "y", "z"]);
    const results = [];
    points.forEach((point) => {
      const vecToPoint = [point.x - origin[0], point.y - origin[1], point.z - origin[2]];
      const crossProd = [
        direction[1] * vecToPoint[2] - direction[2] * vecToPoint[1],
        direction[2] * vecToPoint[0] - direction[0] * vecToPoint[2],
        direction[0] * vecToPoint[1] - direction[1] * vecToPoint[0]
      ];
      const distance = Math.sqrt(crossProd.reduce((sum, val) => sum + val * val, 0));
      if (distance < 0.1) {
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
    const offsetX = point.x - cameraPos[0];
    const offsetY = point.y - cameraPos[1];
    const dot = offsetX * lookDir[0] + offsetY * lookDir[1];
    return {
      x: dot * 100,
      // 简化的屏幕坐标转换
      y: (point.z - cameraPos[2]) * 50
    };
  }
  // 添加到 HypercubeDB 类中
  calculateIntersectionPoints(dimensions, func, sphereRadius = 1, resolution = 0.1) {
    const points = [];
    const min = -sphereRadius;
    const max = sphereRadius;
    const step = resolution;
    if (dimensions.length === 1) {
      for (let x = min; x <= max; x += step) {
        try {
          const result = func(x);
          if (Math.abs(result) < step) {
            const point = {};
            point[dimensions[0]] = x;
            points.push(point);
          }
        } catch (error) {
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
      points.forEach((point) => {
        this.insertData(tableName, point);
      });
      console.log(`Inserted ${points.length} intersection points`);
      log("INFO", `Intersection points inserted: ${points.length}`);
    } catch (error) {
      handleError(error, "inserting intersection points");
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
      this.isDroppingDatabase = true;
      const dbLock = this.locks.get("_global") || /* @__PURE__ */ new Map();
      dbLock.set(databaseName, true);
      try {
        this.data.delete(databaseName);
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
      this.saveData();
      const dbFilePath = path.join(__dirname, `${databaseName}.db`);
      try {
        if (fs.existsSync(dbFilePath)) {
          fs.unlinkSync(dbFilePath);
          console.log(`Database "${databaseName}" deleted`);
        }
        const tempPath = path.join(__dirname, `${databaseName}.db.tmp`);
        if (fs.existsSync(tempPath)) {
          fs.unlinkSync(tempPath);
        }
      } catch (fileError) {
        console.log(`File cleanup error: ${fileError.message}`);
      }
      this.isDroppingDatabase = false;
    } catch (error) {
      handleError(error, "dropping database");
    }
  }
  createPyramidIndex(tableName, maxCapacity = 100, k = 5) {
    if (!this.currentDatabase || !this.data.has(this.currentDatabase) || !this.data.get(this.currentDatabase).has(tableName)) {
      console.log("Table does not exist");
      return;
    }
    const partitionedTable = this.data.get(this.currentDatabase).get(tableName);
    let data = [];
    partitionedTable.partitions.forEach((partition) => {
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
    partitionedTable.partitions.forEach((partition) => {
      if (partition) {
        for (const value of partition.values()) {
          data.push(value);
        }
      }
    });
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
      this.saveData();
      console.log(`Table ${sourceTable} cloned to ${targetTable}`);
      log("INFO", `Table ${sourceTable} cloned to ${targetTable} successfully`);
    } catch (error) {
      handleError(error, "cloning table");
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
      const clonedDatabaseData = /* @__PURE__ */ new Map();
      sourceDatabaseData.forEach((tableData, tableName) => {
        clonedDatabaseData.set(tableName, JSON.parse(JSON.stringify(tableData)));
      });
      this.data.set(targetDatabase, clonedDatabaseData);
      this.saveData();
      console.log(`Database ${sourceDatabase} cloned to ${targetDatabase}`);
      log("INFO", `Database ${sourceDatabase} cloned to ${targetDatabase} successfully`);
    } catch (error) {
      handleError(error, "cloning database");
    }
  }
  registerExtension(extension) {
    if (extension.name && typeof extension.execute === "function") {
      this.extensions[extension.name] = extension;
      log("INFO", `Extension '${extension.name}' registered`);
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
    const db2 = this.data.get(this.currentDatabase);
    if (!db2 || !db2.has(tableName)) {
      console.log("Table does not exist");
      return;
    }
    const table = db2.get(tableName);
    if (table instanceof PartitionedTable) {
      table.compressed = false;
      this.compressionDisabled = true;
      console.log(`Compression disabled for table ${tableName}`);
    }
  }
  setnullcomposite(tableName) {
    if (this.indexes && this.indexes.has(tableName)) {
      for (const [key, indexObj] of this.indexes.get(tableName).entries()) {
        if (indexObj instanceof DimensionsTree) {
          this.indexes.get(tableName).delete(key);
        }
      }
    }
    this.dimensiontree = null;
    console.log(`Composite index for table ${tableName} destroyed`);
  }
  setnullpyramid() {
    this.pyramidIndex = null;
    console.log("Pyramid index destroyed");
  }
  setnullrecursive() {
    this.recursiveSphereWeaving = null;
    console.log("Recursive sphere weaving index destroyed");
  }
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
      partitionedTable.partitions.forEach((partition) => {
        if (partition) {
          for (const data of partition.values()) {
            allData.push(data);
          }
        }
      });
      const dataPoints = allData.map((data) => {
        const point = dimensions.map((dim) => {
          const val = data[dim];
          return val !== void 0 && !isNaN(val) ? parseFloat(val) : 0;
        });
        return { original: data, point };
      }).filter((item) => item.point.some((val) => !isNaN(val)));
      if (dataPoints.length === 0) {
        console.log("No valid numeric data points to cluster");
        return;
      }
      if (k > dataPoints.length) {
        console.log(`k (${k}) cannot be greater than number of data points (${dataPoints.length})`);
        return;
      }
      const points = dataPoints.map((item) => item.point);
      const result = this.performKMeans(points, k, maxIterations);
      result.clusters.forEach((cluster, clusterIndex) => {
        cluster.forEach((pointIndex) => {
          if (pointIndex < dataPoints.length) {
            dataPoints[pointIndex].original.cluster_label = clusterIndex;
          }
        });
      });
      this.saveData();
      console.log(`K-means clustering completed with ${k} clusters`);
      console.log(`Centroids:`, result.centroids);
      log("INFO", "K-means clustering completed successfully");
      return result;
    } catch (error) {
      handleError(error, "k-means clustering");
    }
  }
  performKMeans(points, k, maxIterations = 100) {
    if (points.length === 0 || k <= 0) {
      return { centroids: [], clusters: [] };
    }
    let centroids = this.initializeCentroids(points, k);
    let clusters = [];
    for (let iteration = 0; iteration < maxIterations; iteration++) {
      clusters = this.assignClusters(points, centroids);
      const newCentroids = this.calculateCentroids(clusters, points);
      if (this.haveConverged(centroids, newCentroids)) {
        log("INFO", `K-means converged after ${iteration + 1} iterations`);
        break;
      }
      centroids = newCentroids;
    }
    return { centroids, clusters };
  }
};
var PyramidIndex = class _PyramidIndex {
  constructor(data, maxCapacity = 2, k = 5) {
    this.data = Array.isArray(data) ? data : [];
    this.maxCapacity = maxCapacity;
    this.k = k;
    this.dimensions = this.data.length > 0 ? Array.isArray(this.data[0]) ? this.data[0].length : Object.values(this.data[0]).filter((v) => typeof v === "number" || !isNaN(parseFloat(v))).length : 0;
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
    let values = data.map(
      (item) => Array.isArray(item) ? item[dim] : Object.values(item).filter((v) => typeof v === "number" || !isNaN(parseFloat(v)))[dim]
    ).filter((v) => typeof v !== "string" && v !== void 0 && !isNaN(v));
    if (values.length === 0) {
      return [data];
    }
    const sorted = [...values].sort((a, b) => a - b);
    const median = sorted[Math.floor(sorted.length / 2)];
    const left = [];
    const right = [];
    data.forEach((item) => {
      let val = Array.isArray(item) ? item[dim] : Object.values(item).filter((v) => typeof v === "number" || !isNaN(parseFloat(v)))[dim];
      if (typeof val === "string" || val === void 0 || isNaN(val)) {
        val = median;
      }
      if (val < median) {
        left.push(item);
      } else {
        right.push(item);
      }
    });
    return [
      ...this._recursivePartition(left, depth + 1),
      ...this._recursivePartition(right, depth + 1)
    ];
  }
  static euclideanDistance(a, b) {
    if (!Array.isArray(a) || !Array.isArray(b)) {
      return Infinity;
    }
    let sum = 0;
    for (let i = 0; i < Math.min(a.length, b.length); i++) {
      let va = a[i], vb = b[i];
      if (typeof va === "string" || va === void 0 || isNaN(va)) {
        va = 0;
      }
      if (typeof vb === "string" || vb === void 0 || isNaN(vb)) {
        vb = 0;
      }
      va = parseFloat(va);
      vb = parseFloat(vb);
      sum += Math.pow(va - vb, 2);
    }
    return Math.sqrt(sum);
  }
  // 递归查找目标分区路径，支持回溯
  _findPartitionPath(partitions, queryPoint, depth = 0, k = 5) {
    if (!Array.isArray(partitions) || partitions.length === 0) {
      return [];
    }
    if (!Array.isArray(partitions[0])) {
      return [partitions];
    }
    const dim = depth % this.dimensions;
    let minDist = Infinity, targetIdx = 0;
    partitions.forEach((sub, idx) => {
      if (sub.length === 0) {
        return;
      }
      let center = Array.isArray(sub[0]) ? sub[0] : Object.values(sub[0]).filter((v) => typeof v === "number" || !isNaN(parseFloat(v)));
      const dist = _PyramidIndex.euclideanDistance(center, queryPoint);
      if (dist < minDist) {
        minDist = dist;
        targetIdx = idx;
      }
    });
    const subPartition = partitions[targetIdx];
    if (!Array.isArray(subPartition[0])) {
      const leaves = [];
      leaves.push(subPartition);
      if (targetIdx > 0) {
        leaves.push(partitions[targetIdx - 1]);
      }
      if (targetIdx < partitions.length - 1) {
        leaves.push(partitions[targetIdx + 1]);
      }
      const totalCount = leaves.reduce((sum, arr) => sum + arr.length, 0);
      if (totalCount >= k || depth === 0) {
        return leaves;
      } else {
        return partitions;
      }
    } else {
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
    this.data = this.data.filter((d) => !this._isSame(d, item));
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
    let candidatePartitions = this._findPartitionPath(this.partitions, queryPoint, 0, this.k);
    let candidates = candidatePartitions.flat();
    if (candidates.length < this.k) {
      candidates = this.data;
    }
    const results = candidates.map((item) => {
      let pointArr = Array.isArray(item) ? item : Object.values(item).filter((v) => typeof v === "number" || !isNaN(parseFloat(v)));
      return {
        item,
        dist: _PyramidIndex.euclideanDistance(pointArr, queryPoint)
      };
    }).sort((a, b) => a.dist - b.dist).slice(0, this.k).map((r) => r.item);
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
    } else if (typeof a === "object" && typeof b === "object") {
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
};
var RecursiveSphereWeaving = class {
  constructor(data, riemannSphereDB, dimensions = null) {
    this.data = data;
    this.riemannSphereDB = riemannSphereDB;
    this.dimensions = dimensions || (data.length > 0 ? Object.keys(data[0]) : []);
    this.spherePoints = data.map((row) => {
      const vec = this.dimensions.map((dim) => {
        const v = row[dim];
        return typeof v === "string" || v === void 0 || isNaN(v) ? 0 : parseFloat(v);
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
    const vec = this.dimensions.map((dim) => {
      const v = row[dim];
      return typeof v === "string" || v === void 0 || isNaN(v) ? 0 : parseFloat(v);
    });
    this.spherePoints.push({
      original: row,
      sphere: this.riemannSphereDB.mapToRiemannSphere(vec)
    });
  }
  delete(row) {
    this.spherePoints = this.spherePoints.filter((pt) => pt.original !== row);
  }
  update(oldRow, newRow) {
    this.delete(oldRow);
    this.insert(newRow);
  }
  // 查询：输入高维点，返回球面空间最近的若干原始数据
  query(queryPoint, k = 5) {
    let vec;
    if (Array.isArray(queryPoint)) {
      vec = queryPoint;
    } else if (typeof queryPoint === "object") {
      vec = this.dimensions.map((dim) => parseFloat(queryPoint[dim]) || 0);
    } else {
      throw new Error("Invalid query point");
    }
    const sphereQuery = this.riemannSphereDB.mapToRiemannSphere(vec);
    const results = this.spherePoints.map((item) => ({
      original: item.original,
      dist: this.euclideanDistance(item.sphere, sphereQuery)
    })).sort((a, b) => a.dist - b.dist).slice(0, k).map((r) => r.original);
    return results;
  }
};
var RiemannSphereDB = class {
  constructor() {
    this.data = /* @__PURE__ */ new Map();
    this.mappedData = /* @__PURE__ */ new Map();
    this.sphereCenter = [0, 0, 0];
    this.compressionRate = 0.7;
    this.spatialIndex = /* @__PURE__ */ new Map();
  }
  // 只保留这一个实现
  mapToRiemannSphere(dataPoint) {
    if (!Array.isArray(dataPoint) || dataPoint.length < 2) {
      return [0, 0, -1];
    }
    const v0 = typeof dataPoint[0] === "string" || dataPoint[0] === void 0 || isNaN(dataPoint[0]) ? 0 : parseFloat(dataPoint[0]);
    const v1 = typeof dataPoint[1] === "string" || dataPoint[1] === void 0 || isNaN(dataPoint[1]) ? 0 : parseFloat(dataPoint[1]);
    const theta = Math.atan(v0) * 180 / Math.PI;
    const phi = Math.atan(v1) * 180 / Math.PI;
    const thetaRad = theta * Math.PI / 180;
    const phiRad = phi * Math.PI / 180;
    const r = 1;
    const x = r * Math.cos(thetaRad) * Math.cos(phiRad);
    const y = r * Math.cos(thetaRad) * Math.sin(phiRad);
    const z = r * Math.sin(thetaRad);
    return [x, y, z];
  }
  buildSpatialIndex() {
    this.spatialIndex.clear();
    this.mappedData.forEach((mappedPoint, key) => {
      const gridKey = mappedPoint.slice(0, 3).map((v) => Math.floor(v * 10)).join("_");
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
    const crypto2 = require("crypto");
    return crypto2.createHash("sha256").update(JSON.stringify(dataPoint)).digest("hex");
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
          if (query.distance !== void 0) {
            const distance = query.distance;
            const bucketSize = 0.1;
            const bucketKey = Math.floor(distance / bucketSize) * bucketSize;
            if (this.distanceHashes[tableName][bucketKey]) {
              results = this.distanceHashes[tableName][bucketKey].filter((data) => {
                const pointDistance = Math.sqrt(Object.keys(data).reduce((sum, dim) => sum + Math.pow(data[dim], 2), 0));
                return pointDistance >= distance - bucketSize && pointDistance < distance + bucketSize;
              });
            } else {
              results = [];
            }
          } else {
            const indexKeys = Object.keys(this.indexes[tableName]).filter((indexKey) => {
              return indexKey.split("_").every((dim) => query[dim] !== void 0);
            });
            if (indexKeys.length > 0) {
              const indexKey = indexKeys[0];
              const indexValues = indexKey.split("_").map((dim) => query[dim]);
              const indexValue = indexValues.join("_");
              results = this.indexes[tableName][indexKey][indexValue] || [];
            } else {
              const filterFunction = this.parseQuery(query);
              results = results.filter(filterFunction);
            }
          }
          if (query.label !== void 0) {
            results = results.filter((data) => data.label === query.label);
          }
          console.log(results);
        } finally {
          this.releaseLock(tableName);
        }
      } else {
        console.log("Table is locked");
      }
      log("INFO", "Data selected successfully");
    } catch (error) {
      handleError(error, "selecting data");
    }
  }
  isMatch(data, query) {
    try {
      if (!query || Object.keys(query).length === 0) {
        return true;
      }
      return Object.entries(query).every(([field, condition]) => {
        console.log(condition);
        if (typeof condition === "object" && condition !== null) {
          if (condition.hasOwnProperty("_subquery")) {
            const subQueryResult = this._executeSubQuery(condition._subquery);
            if (!Array.isArray(subQueryResult) || subQueryResult.length !== 1) {
              throw new Error("Subquery must return exactly one row");
            }
            const columnValue = subQueryResult[0][condition._column];
            if (columnValue === void 0) {
              throw new Error(`Column ${condition._column} not found in subquery result`);
            }
            console.log("/n");
            console.log(columnValue);
            console.log("/n");
            return data[field] === columnValue;
          }
          if (condition._op) {
            switch (condition._op.toLowerCase()) {
              case "in":
                return Array.isArray(condition._value) && condition._value.includes(data[field]);
              case "between":
                return Array.isArray(condition._value) && condition._value.length === 2 && data[field] >= condition._value[0] && data[field] <= condition._value[1];
              case "like":
                const pattern = new RegExp(
                  condition._value.replace(/%/g, ".*").replace(/_/g, "."),
                  "i"
                );
                return pattern.test(String(data[field]));
              default:
                throw new Error(`Unsupported operator: ${condition._op}`);
            }
          }
          if (condition._column && condition._table) {
            const refValue = this._getReferencedValue(
              condition._table,
              condition._column,
              data
            );
            return data[field] === refValue;
          }
        }
        return data[field] === condition;
      });
    } catch (error) {
      log("ERROR", `Error in isMatch: ${error.message}`);
      return false;
    }
  }
  // 执行子查询
  _executeSubQuery(subquery) {
    try {
      const result = this.query(subquery);
      if (!Array.isArray(result)) {
        throw new Error("Subquery execution failed");
      }
      return result;
    } catch (error) {
      log("ERROR", `Subquery execution failed: ${error.message}`);
      throw error;
    }
  }
  // 获取引用列的值
  _getReferencedValue(tableName, columnName, currentRow) {
    try {
      const refTable = this._findTable(tableName);
      if (!refTable) {
        throw new Error(`Referenced table ${tableName} not found`);
      }
      const result = refTable.query({
        [columnName]: currentRow[columnName]
      });
      if (!Array.isArray(result) || result.length !== 1) {
        throw new Error(`Invalid reference result for ${tableName}.${columnName}`);
      }
      return result[0][columnName];
    } catch (error) {
      log("ERROR", `Reference resolution failed: ${error.message}`);
      throw error;
    }
  }
  riemannKMeansClustering(k) {
    const lowDimPoints = Object.values(this.mappedData);
    const clusters = this.performKMeans(lowDimPoints, k);
    const highDimClusters = clusters.map((cluster) => {
      return cluster.map((mappedPoint) => {
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
    let centroids = this.initializeCentroids(points, k);
    let clusters = [];
    let previousClusters = [];
    for (let iteration = 0; iteration < maxIterations; iteration++) {
      clusters = this.assignClusters(points, centroids);
      const newCentroids = this.calculateCentroids(clusters, points);
      if (this.haveConverged(centroids, newCentroids)) {
        log("INFO", `K-means converged after ${iteration + 1} iterations`);
        break;
      }
      centroids = newCentroids;
      previousClusters = clusters;
    }
    return clusters;
  }
  visualizeData() {
    const points = Object.values(this.mappedData);
    const visualizationData = points.map((point) => ({
      x: point[0],
      y: point[1],
      z: point[2]
      // 假设映射到三维空间
    }));
    return visualizationData;
  }
};
var DimensionsTree = class {
  constructor(dimensions) {
    this.root = /* @__PURE__ */ new Map();
    this.dimensions = dimensions;
  }
  // 插入数据点
  insert(keys, value, node = this.root, depth = 0) {
    if (depth === this.dimensions.length) {
      if (!node.has("_data")) {
        node.set("_data", []);
      }
      node.get("_data").push(value);
      return;
    }
    const key = String(keys[depth]);
    if (!node.has(key)) {
      node.set(key, /* @__PURE__ */ new Map());
    }
    this.insert(keys, value, node.get(key), depth + 1);
  }
  // 查询数据点
  query(range, node = this.root, depth = 0, results = []) {
    if (depth === this.dimensions.length) {
      if (node.has("_data")) {
        results.push(...node.get("_data"));
      }
      return results;
    }
    const [min, max] = range[depth];
    const minStr = String(min);
    const maxStr = String(max);
    for (const [key, childNode] of node.entries()) {
      if (key == "_data") {
        continue;
      }
      if (key >= minStr && key <= maxStr) {
        this.query(range, childNode, depth + 1, results);
      }
    }
    return results;
  }
  // 在 DimensionsTree 类中添加
  delete(keys, value, node = this.root, depth = 0) {
    if (depth === this.dimensions.length) {
      if (node.has("_data")) {
        node.set("_data", node.get("_data").filter((v) => v !== value));
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
      if (node.has("_data")) {
        const data = node.get("_data");
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
};
var { Worker, isMainThread, parentPort, workerData } = require("worker_threads");
var { stringify } = require("querystring");
function computeClusters(dataPoints, k, maxIterations) {
  function euclideanDistance(a, b) {
    return Math.sqrt(a.reduce((sum, val, i) => sum + (val - b[i]) ** 2, 0));
  }
  function initializeCentroids(dataPoints2, k2) {
    const indices = Array.from({ length: dataPoints2.length }, (_, i) => i);
    const randomIndices = indices.sort(() => Math.random() - 0.5).slice(0, k2);
    return randomIndices.map((index) => dataPoints2[index]);
  }
  function assignClusters(dataPoints2, centroids2) {
    const clusters2 = Array.from({ length: k }, () => []);
    dataPoints2.forEach((point, index) => {
      const distances = centroids2.map((centroid) => euclideanDistance(point, centroid));
      const closestCentroidIndex = distances.indexOf(Math.min(...distances));
      clusters2[closestCentroidIndex].push(index);
    });
    return clusters2;
  }
  function calculateCentroids(clusters2, dataPoints2) {
    return clusters2.map((cluster) => {
      if (cluster.length === 0) {
        return dataPoints2[Math.floor(Math.random() * dataPoints2.length)];
      }
      const sum = cluster.reduce((acc, index) => {
        return acc.map((val, i) => val + dataPoints2[index][i]);
      }, Array(dataPoints2[0].length).fill(0));
      return sum.map((val) => val / cluster.length);
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
async function executeScriptFile(filePath) {
  try {
    const data = fs.readFileSync(filePath, "utf8");
    const lines = data.split("\n");
    for (const line of lines) {
      const trimmedLine = line.trim();
      if (trimmedLine && !trimmedLine.startsWith("#")) {
        executePrompt(trimmedLine);
      }
    }
    console.log(`Script file '${filePath}' executed successfully`);
    log("INFO", `Script file '${filePath}' executed successfully`);
  } catch (error) {
    handleError(error, "executing script file");
  }
}
function parseConditions(conditionStr) {
  if (typeof conditionStr !== "string") {
    throw new Error("Condition must be a string");
  }
  if (conditionStr.toLowerCase().includes(" and ")) {
    return {
      _op: "AND",
      _conditions: conditionStr.split(/\s+and\s+/i).map((c) => parseConditions(c))
    };
  }
  if (conditionStr.toLowerCase().includes(" or ")) {
    return {
      _op: "OR",
      _conditions: conditionStr.split(/\s+or\s+/i).map((c) => parseConditions(c))
    };
  }
  if (conditionStr.toLowerCase().startsWith("not ")) {
    return {
      _op: "NOT",
      _condition: parseConditions(conditionStr.substring(4))
    };
  }
  if (conditionStr.includes("{") && conditionStr.includes("}")) {
    const subqueryMatch = /^([a-zA-Z0-9_]+)\s*=\s*\{([^}]+)\}$/.exec(conditionStr);
    if (subqueryMatch) {
      const [_2, field2, subqueryParts] = subqueryMatch;
      const [column, subquery] = subqueryParts.split(",").map((s) => s.trim());
      return {
        [field2]: {
          column: column.split(":")[1],
          subquery: subquery.split(":")[1]
        }
      };
    }
  }
  const operatorPattern = /^([a-zA-Z0-9_]+)\s*(=|>|<|>=|<=|<>)\s*([^=<>]+)$/;
  const match = operatorPattern.exec(conditionStr.trim());
  if (!match) {
    throw new Error("Invalid condition format");
  }
  const [_, field, operator, value] = match;
  return parseBasicCondition(field, operator, value.trim());
}
function parseBasicCondition(field, operator, value) {
  if (!field || !operator) {
    throw new Error("Missing field name or operator");
  }
  const validOperators = ["=", ">", "<", ">=", "<=", "<>"];
  if (!validOperators.includes(operator)) {
    throw new Error(`Invalid operator: ${operator}`);
  }
  let parsedValue;
  if (value === void 0 || value === null) {
    throw new Error(`Missing value for condition: ${field} ${operator}`);
  }
  parsedValue = value.startsWith('"') || value.startsWith("'") ? value.slice(1, -1) : isNaN(value) ? value : Number(value);
  return {
    [field]: {
      _op: operator,
      _value: parsedValue
    }
  };
}
if (!isMainThread) {
  const { dataPoints, k, maxIterations } = workerData;
  const result = computeClusters(dataPoints, k, maxIterations);
  parentPort.postMessage(result);
}
function generateDataPoints(distributionType, params) {
  const dataPoints = [];
  const numPoints = params.numPoints || 100;
  try {
    switch (distributionType.toLowerCase()) {
      case "uniform":
        for (let i = 0; i < numPoints; i++) {
          const point = {};
          Object.keys(params).forEach((key) => {
            if (key !== "numPoints" && params[key].min !== void 0 && params[key].max !== void 0) {
              point[key] = Math.random() * (params[key].max - params[key].min) + params[key].min;
            }
          });
          if (Object.keys(point).length > 0) {
            point.id = i;
            dataPoints.push(point);
          }
        }
        break;
      case "normal":
        for (let i = 0; i < numPoints; i++) {
          const point = {};
          Object.keys(params).forEach((key) => {
            if (key !== "numPoints" && params[key].mean !== void 0) {
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
      case "sphere":
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
function normalRandom(mean = 0, stdDev = 1) {
  let u = 0, v = 0;
  while (u === 0) {
    u = Math.random();
  }
  while (v === 0) {
    v = Math.random();
  }
  const z = Math.sqrt(-2 * Math.log(u)) * Math.cos(2 * Math.PI * v);
  return z * stdDev + mean;
}
var db = new HypercubeDB();
global.hypercubeDB = db;
function displayPrompt() {
  process.stdout.write(">");
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
  const reg11 = new RegExp("^\\s*import\\s+csv\\s+([a-zA-Z0-9_]+)\\s+from\\s+([^\\s]+)\\s*$");
  const regUpdate = /^\s*update\s+([a-zA-Z0-9_]+)\s+set\s+([a-zA-Z0-9_]+)\s*=\s*([^\s]+)\s+where\s+(.+)\s*$/i;
  const regUpdateBatchColumn = /^\s*update\s+batch\s+column\s+([a-zA-Z0-9_]+)\s+set\s*\{([^\}]+)\}\s+where\s+(.+)\s*$/i;
  const regDelete = /^\s*delete\s+from\s+([a-zA-Z0-9_]+)\s+where\s+(.+)\s*$/i;
  const reg13 = new RegExp("^\\s*update\\s+batch\\s+([a-zA-Z0-9_]+)\\s+set\\s+\\[([^]]+)\\]\\s*$");
  const reg15 = new RegExp("^\\s*drop\\s+table\\s+([a-zA-Z0-9_]+)\\s*$");
  const regSelectData = new RegExp(
    "^\\s*select\\s+\\*\\s+from\\s+([a-zA-Z0-9_]+)(?:\\s+where\\s+(.+))?$",
    "i"
  );
  const reg16 = /^\s*load\s+extension\s+([^\s]+)\s*$/i;
  const reg17 = /^\s*extension\s+(\S+)/i;
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
  const reg29 = /^\s*execute\s+script\s+([^\s]+)\s*$/i;
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
  const reg35 = /^\s*--/;
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
      const schema = schemaStr.split(",");
      const partitionKey = match[5] || "id";
      const numPartitions = match[8] ? parseInt(match[8]) : 10;
      db.createTable(tableName, schema, partitionKey, numPartitions);
    } else {
      console.log("Invalid command format for create table");
    }
  } else if (reg4.test(answer)) {
    const tableName = reg4.exec(answer)[1];
    const dataStr = reg4.exec(answer)[2];
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
      const formattedStr = dataArrayStr.replace(/([a-zA-Z0-9_]+):/g, '"$1":').replace(/'/g, '"');
      const dataArray = JSON.parse(`[${formattedStr}]`);
      db.insertBatch(tableName, dataArray);
    } catch (error) {
      console.log("Invalid JSON format:", error.message);
      console.log('\u63D0\u793A: JSON\u683C\u5F0F\u9700\u8981\u952E\u540D\u4F7F\u7528\u53CC\u5F15\u53F7\uFF0C\u4F8B\u5982: {"id":1,"name":"Alice"}');
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
  } else if (reg11.test(answer)) {
    const tableName = reg11.exec(answer)[1];
    const filePath = reg11.exec(answer)[2];
    db.importFromCSV(tableName, filePath);
  } else if (regUpdate.test(answer)) {
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
  } else if (regUpdateBatchColumn.test(answer)) {
    const match = regUpdateBatchColumn.exec(answer);
    const tableName = match[1];
    const columnsStr = match[2];
    const conditionStr = match[3];
    let newData = {};
    columnsStr.split(",").forEach((pair) => {
      const [col, val] = pair.split("=").map((s) => s.trim());
      newData[col] = isNaN(val) ? val : parseFloat(val);
    });
    let query = {};
    try {
      query = parseConditions(conditionStr);
    } catch (error) {
      console.log(error.message);
      return;
    }
    db.updateBatchColumn(tableName, query, newData);
  } else if (regDelete.test(answer)) {
    const match = regDelete.exec(answer);
    const tableName = match[1];
    const conditionStr = match[2];
    try {
      const query = parseConditions(conditionStr);
      db.deleteData(tableName, query);
    } catch (error) {
      console.log(error.message);
    }
  } else if (reg13.test(answer)) {
    const match = reg13.exec(answer);
    const tableName = match[1];
    const updatesStr = match[2];
    try {
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
    let queryStr = match[2];
    let query = {};
    if (queryStr && typeof queryStr === "string") {
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
    let queryStr = match[3];
    let query = {};
    if (queryStr && typeof queryStr === "string") {
      try {
        query = parseConditions(queryStr);
      } catch (error) {
        console.log(error.message);
        return;
      }
    }
    let fields = ["*"];
    if (fieldsStr !== "*") {
      fields = fieldsStr.split(",").map((f) => f.trim()).filter((f) => f.length > 0);
    }
    db.selectData(tableName, query, fields);
  } else if (reg16.test(answer)) {
    const extensionPath = reg16.exec(answer)[1];
    db.loadExtension(extensionPath);
  } else if (reg17.test(answer)) {
    const match = reg17.exec(answer);
    const extensionName = match[1].toLocaleLowerCase();
    if (!/^[a-zA-Z0-9_-]+$/.test(extensionName)) {
      console.log("\u65E0\u6548\u6269\u5C55\u540D\u683C\u5F0F\uFF0C\u53EA\u80FD\u5305\u542B\u5B57\u6BCD\u3001\u6570\u5B57\u3001\u4E0B\u5212\u7EBF\u548C\u8FDE\u5B57\u7B26");
      return;
    }
    let args = [];
    const argStr = answer.slice(answer.indexOf(extensionName) + extensionName.length).trim();
    if (argStr) {
      args = argStr.split(" ").filter((arg) => arg.trim() !== "");
    }
    db.executeExtension(extensionName, ...args);
  } else if (reg18.test(answer)) {
    const match = reg18.exec(answer);
    const tableName = match[1];
    const dimensionsStr = match[2];
    const dimensions = dimensionsStr.split(",");
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
    const dimensions = match[2].split(",");
    const points = db.projectToRiemannSphere(tableName, dimensions);
    console.log(points.map(
      (p) => `(${p.x.toFixed(2)}, ${p.y.toFixed(2)}, ${p.z.toFixed(2)})`
    ).join("\n"));
    generateWebView(points);
  } else if (reg23.test(answer)) {
    const match = reg23.exec(answer);
    const tableName = match[1];
    const dimensionsStr = match[2];
    const funcStr = match[3];
    const sphereRadius = parseFloat(match[4]);
    const resolution = parseFloat(match[6]);
    const dimensions = dimensionsStr.split(",");
    const func = new Function(...dimensions, `return ${funcStr};`);
    db.insertIntersectionPoints(tableName, dimensions, func, sphereRadius, resolution);
  } else if (reg24.test(answer)) {
    const match = reg24.exec(answer);
    const tableName = match[1];
    const dimensionsStr = match[2];
    const k = parseInt(match[3]);
    const dimensions = dimensionsStr.split(",");
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
    const point = pointStr.split(",").map(Number);
    db.queryPyramidIndex(point);
  } else if (reg27.test(answer)) {
    const match = reg27.exec(answer);
    const tableName = match[1];
    db.createRecursiveSphereWeaving(tableName);
  } else if (reg28.test(answer)) {
    const match = reg28.exec(answer);
    const tableName = match[1];
    const pointStr = match[2];
    const point = pointStr.split(",").map(Number);
    db.queryRecursiveSphereWeaving(point);
  } else if (regQueryRSW.test(answer)) {
    const match = regQueryRSW.exec(answer);
    const tableName = match[1];
    const pointStr = match[2];
    const k = match[3] ? parseInt(match[3], 10) : 5;
    const point = pointStr.split(",").map((v) => parseFloat(v.trim()));
    try {
      db.queryRecursiveSphereWeaving(tableName, point, k);
    } catch (error) {
      console.log(error.message);
    }
  } else if (reg29.test(answer)) {
    const scriptPath = reg29.exec(answer)[1];
    executeScriptFile(scriptPath);
  } else if (regQueryCompositeIndex.test(answer)) {
    const match = regQueryCompositeIndex.exec(answer);
    const tableName = match[1];
    const dimsStr = match[2];
    let dimsStrFixed = dimsStr.replace(/([a-zA-Z0-9_]+)\s*:/g, '"$1":').replace(/'/g, '"');
    let dims = {};
    try {
      dimsStrFixed = dimsStrFixed.replace(/([a-zA-Z0-9_]+)\s*:/g, '"$1":').replace(/'/g, '"');
      dims = JSON.parse(`{${dimsStrFixed}}`);
    } catch (e) {
      console.log("\u7EF4\u5EA6\u53C2\u6570\u89E3\u6790\u5931\u8D25:", e.message);
      return;
    }
    db.queryCompositeIndexRange(tableName, dims);
  } else if (regEnableCompression.test(answer)) {
    const tableName = regEnableCompression.exec(answer)[1];
    if (db.data.has(db.currentDatabase) && db.data.get(db.currentDatabase).has(tableName)) {
      const table = db.data.get(db.currentDatabase).get(tableName);
      table.compressed = true;
      console.log(`Compression enabled for table ${tableName}`);
    } else {
      console.log(`Table ${tableName} does not exist`);
    }
  } else if (regDisableCompression.test(answer)) {
    const tableName = regDisableCompression.exec(answer)[1];
    if (db.data.has(db.currentDatabase) && db.data.get(db.currentDatabase).has(tableName)) {
      const table = db.data.get(db.currentDatabase).get(tableName);
      table.compressed = false;
      console.log(`Compression disabled for table ${tableName}`);
    } else {
      console.log(`Table ${tableName} does not exist`);
    }
  } else if (regGeneratePoints.test(answer)) {
    const match = regGeneratePoints.exec(answer);
    const tableName = match[1];
    const distributionType = match[2];
    const parameters = match[3];
    const params = JSON.parse(`{${parameters}}`);
    const dataPoints = generateDataPoints(distributionType, params);
    if (db.data.has(db.currentDatabase) && db.data.get(db.currentDatabase).has(tableName)) {
      const table = db.data.get(db.currentDatabase).get(tableName);
      if (table.compressed) {
        const compressedPoints = dataPoints.map((point) => db.conicalProjectionCompress(point));
        db.insertBatch(tableName, compressedPoints);
        console.log(`Generated and compressed ${dataPoints.length} points into table ${tableName}`);
      } else {
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
    db.setnullpyramid();
  } else if (reg32.test(answer)) {
    db.setnullrecursive();
  } else if (reg33.test(answer)) {
    const extensionName = reg33.exec(answer)[1];
    db.removeExtension(extensionName);
  } else if (reg34.test(answer)) {
    db.showExtensions();
  } else if (reg35.test(answer)) {
  } else if (regMount.test(answer)) {
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
  fs.writeFileSync("points.json", data);
  console.log("Visualization data saved: points.json");
}
rl.on("line", (answer) => {
  executePrompt(answer);
});
rl.on("close", () => {
  if (!exports.isDroppingDatabase) {
    db.saveData();
  }
  console.log("thank you for using Devilfish DBMS system, see you next time!");
  process.exit(0);
});
module.exports = { HypercubeDB };
