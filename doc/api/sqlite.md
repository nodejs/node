# SQLite

<!--introduced_in=v22.5.0-->

<!-- YAML
added: v22.5.0
changes:
  - version:
    - v23.4.0
    - v22.13.0
    pr-url: https://github.com/nodejs/node/pull/55890
    description: SQLite is no longer behind `--experimental-sqlite` but still experimental.
-->

> Stability: 1.1 - Active development.

<!-- source_link=lib/sqlite.js -->

The `node:sqlite` module facilitates working with SQLite databases.
To access it:

```mjs
import sqlite from 'node:sqlite';
```

```cjs
const sqlite = require('node:sqlite');
```

This module is only available under the `node:` scheme.

The following example shows the basic usage of the `node:sqlite` module to open
an in-memory database, write data to the database, and then read the data back.

```mjs
import { DatabaseSync } from 'node:sqlite';
const database = new DatabaseSync(':memory:');

// Execute SQL statements from strings.
database.exec(`
  CREATE TABLE data(
    key INTEGER PRIMARY KEY,
    value TEXT
  ) STRICT
`);
// Create a prepared statement to insert data into the database.
const insert = database.prepare('INSERT INTO data (key, value) VALUES (?, ?)');
// Execute the prepared statement with bound values.
insert.run(1, 'hello');
insert.run(2, 'world');
// Create a prepared statement to read data from the database.
const query = database.prepare('SELECT * FROM data ORDER BY key');
// Execute the prepared statement and log the result set.
console.log(query.all());
// Prints: [ { key: 1, value: 'hello' }, { key: 2, value: 'world' } ]
```

```cjs
'use strict';
const { DatabaseSync } = require('node:sqlite');
const database = new DatabaseSync(':memory:');

// Execute SQL statements from strings.
database.exec(`
  CREATE TABLE data(
    key INTEGER PRIMARY KEY,
    value TEXT
  ) STRICT
`);
// Create a prepared statement to insert data into the database.
const insert = database.prepare('INSERT INTO data (key, value) VALUES (?, ?)');
// Execute the prepared statement with bound values.
insert.run(1, 'hello');
insert.run(2, 'world');
// Create a prepared statement to read data from the database.
const query = database.prepare('SELECT * FROM data ORDER BY key');
// Execute the prepared statement and log the result set.
console.log(query.all());
// Prints: [ { key: 1, value: 'hello' }, { key: 2, value: 'world' } ]
```

## Class: `DatabaseSync`

<!-- YAML
added: v22.5.0
changes:
  - version:
    - v24.0.0
    - v22.16.0
    pr-url: https://github.com/nodejs/node/pull/57752
    description: Add `timeout` option.
  - version:
    - v23.10.0
    - v22.15.0
    pr-url: https://github.com/nodejs/node/pull/56991
    description: The `path` argument now supports Buffer and URL objects.
-->

This class represents a single [connection][] to a SQLite database. All APIs
exposed by this class execute synchronously.

### `new DatabaseSync(path[, options])`

<!-- YAML
added: v22.5.0
changes:
  - version:
      - v25.1.0
    pr-url: https://github.com/nodejs/node/pull/60217
    description: Add `defensive` option.
  - version:
      - v24.4.0
      - v22.18.0
    pr-url: https://github.com/nodejs/node/pull/58697
    description: Add new SQLite database options.
-->

* `path` {string | Buffer | URL} The path of the database. A SQLite database can be
  stored in a file or completely [in memory][]. To use a file-backed database,
  the path should be a file path. To use an in-memory database, the path
  should be the special name `':memory:'`.
* `options` {Object} Configuration options for the database connection. The
  following options are supported:
  * `open` {boolean} If `true`, the database is opened by the constructor. When
    this value is `false`, the database must be opened via the `open()` method.
    **Default:** `true`.
  * `readOnly` {boolean} If `true`, the database is opened in read-only mode.
    If the database does not exist, opening it will fail. **Default:** `false`.
  * `enableForeignKeyConstraints` {boolean} If `true`, foreign key constraints
    are enabled. This is recommended but can be disabled for compatibility with
    legacy database schemas. The enforcement of foreign key constraints can be
    enabled and disabled after opening the database using
    [`PRAGMA foreign_keys`][]. **Default:** `true`.
  * `enableDoubleQuotedStringLiterals` {boolean} If `true`, SQLite will accept
    [double-quoted string literals][]. This is not recommended but can be
    enabled for compatibility with legacy database schemas.
    **Default:** `false`.
  * `allowExtension` {boolean} If `true`, the `loadExtension` SQL function
    and the `loadExtension()` method are enabled.
    You can call `enableLoadExtension(false)` later to disable this feature.
    **Default:** `false`.
  * `timeout` {number} The [busy timeout][] in milliseconds. This is the maximum amount of
    time that SQLite will wait for a database lock to be released before
    returning an error. **Default:** `0`.
  * `readBigInts` {boolean} If `true`, integer fields are read as JavaScript `BigInt` values. If `false`,
    integer fields are read as JavaScript numbers. **Default:** `false`.
  * `returnArrays` {boolean} If `true`, query results are returned as arrays instead of objects.
    **Default:** `false`.
  * `allowBareNamedParameters` {boolean} If `true`, allows binding named parameters without the prefix
    character (e.g., `foo` instead of `:foo`). **Default:** `true`.
  * `allowUnknownNamedParameters` {boolean} If `true`, unknown named parameters are ignored when binding.
    If `false`, an exception is thrown for unknown named parameters. **Default:** `false`.
  * `defensive` {boolean} If `true`, enables the defensive flag. When the defensive flag is enabled,
    language features that allow ordinary SQL to deliberately corrupt the database file are disabled.
    The defensive flag can also be set using `enableDefensive()`.
    **Default:** `false`.

Constructs a new `DatabaseSync` instance.

### `database.aggregate(name, options)`

<!-- YAML
added:
 - v24.0.0
 - v22.16.0
-->

Registers a new aggregate function with the SQLite database. This method is a wrapper around
[`sqlite3_create_window_function()`][].

* `name` {string} The name of the SQLite function to create.
* `options` {Object} Function configuration settings.
  * `deterministic` {boolean} If `true`, the [`SQLITE_DETERMINISTIC`][] flag is
    set on the created function. **Default:** `false`.
  * `directOnly` {boolean} If `true`, the [`SQLITE_DIRECTONLY`][] flag is set on
    the created function. **Default:** `false`.
  * `useBigIntArguments` {boolean} If `true`, integer arguments to `options.step` and `options.inverse`
    are converted to `BigInt`s. If `false`, integer arguments are passed as
    JavaScript numbers. **Default:** `false`.
  * `varargs` {boolean} If `true`, `options.step` and `options.inverse` may be invoked with any number of
    arguments (between zero and [`SQLITE_MAX_FUNCTION_ARG`][]). If `false`,
    `inverse` and `step` must be invoked with exactly `length` arguments.
    **Default:** `false`.
  * `start` {number | string | null | Array | Object | Function} The identity
    value for the aggregation function. This value is used when the aggregation
    function is initialized. When a {Function} is passed the identity will be its return value.
  * `step` {Function} The function to call for each row in the aggregation. The
    function receives the current state and the row value. The return value of
    this function should be the new state.
  * `result` {Function} The function to call to get the result of the
    aggregation. The function receives the final state and should return the
    result of the aggregation.
  * `inverse` {Function} When this function is provided, the `aggregate` method will work as a window function.
    The function receives the current state and the dropped row value. The return value of this function should be the
    new state.

When used as a window function, the `result` function will be called multiple times.

```cjs
const { DatabaseSync } = require('node:sqlite');

const db = new DatabaseSync(':memory:');
db.exec(`
  CREATE TABLE t3(x, y);
  INSERT INTO t3 VALUES ('a', 4),
                        ('b', 5),
                        ('c', 3),
                        ('d', 8),
                        ('e', 1);
`);

db.aggregate('sumint', {
  start: 0,
  step: (acc, value) => acc + value,
});

db.prepare('SELECT sumint(y) as total FROM t3').get(); // { total: 21 }
```

```mjs
import { DatabaseSync } from 'node:sqlite';

const db = new DatabaseSync(':memory:');
db.exec(`
  CREATE TABLE t3(x, y);
  INSERT INTO t3 VALUES ('a', 4),
                        ('b', 5),
                        ('c', 3),
                        ('d', 8),
                        ('e', 1);
`);

db.aggregate('sumint', {
  start: 0,
  step: (acc, value) => acc + value,
});

db.prepare('SELECT sumint(y) as total FROM t3').get(); // { total: 21 }
```

### `database.close()`

<!-- YAML
added: v22.5.0
-->

Closes the database connection. An exception is thrown if the database is not
open. This method is a wrapper around [`sqlite3_close_v2()`][].

### `database.loadExtension(path)`

<!-- YAML
added:
  - v23.5.0
  - v22.13.0
-->

* `path` {string} The path to the shared library to load.

Loads a shared library into the database connection. This method is a wrapper
around [`sqlite3_load_extension()`][]. It is required to enable the
`allowExtension` option when constructing the `DatabaseSync` instance.

### `database.enableLoadExtension(allow)`

<!-- YAML
added:
  - v23.5.0
  - v22.13.0
-->

* `allow` {boolean} Whether to allow loading extensions.

Enables or disables the `loadExtension` SQL function, and the `loadExtension()`
method. When `allowExtension` is `false` when constructing, you cannot enable
loading extensions for security reasons.

### `database.enableDefensive(active)`

<!-- YAML
added:
  - v25.1.0
-->

* `active` {boolean} Whether to set the defensive flag.

Enables or disables the defensive flag. When the defensive flag is active,
language features that allow ordinary SQL to deliberately corrupt the database file are disabled.
See [`SQLITE_DBCONFIG_DEFENSIVE`][] in the SQLite documentation for details.

### `database.location([dbName])`

<!-- YAML
added:
  - v24.0.0
  - v22.16.0
-->

* `dbName` {string} Name of the database. This can be `'main'` (the default primary database) or any other
  database that has been added with [`ATTACH DATABASE`][] **Default:** `'main'`.
* Returns: {string | null} The location of the database file. When using an in-memory database,
  this method returns null.

This method is a wrapper around [`sqlite3_db_filename()`][]

### `database.exec(sql)`

<!-- YAML
added: v22.5.0
-->

* `sql` {string} A SQL string to execute.

This method allows one or more SQL statements to be executed without returning
any results. This method is useful when executing SQL statements read from a
file. This method is a wrapper around [`sqlite3_exec()`][].

### `database.function(name[, options], function)`

<!-- YAML
added:
  - v23.5.0
  - v22.13.0
-->

* `name` {string} The name of the SQLite function to create.
* `options` {Object} Optional configuration settings for the function. The
  following properties are supported:
  * `deterministic` {boolean} If `true`, the [`SQLITE_DETERMINISTIC`][] flag is
    set on the created function. **Default:** `false`.
  * `directOnly` {boolean} If `true`, the [`SQLITE_DIRECTONLY`][] flag is set on
    the created function. **Default:** `false`.
  * `useBigIntArguments` {boolean} If `true`, integer arguments to `function`
    are converted to `BigInt`s. If `false`, integer arguments are passed as
    JavaScript numbers. **Default:** `false`.
  * `varargs` {boolean} If `true`, `function` may be invoked with any number of
    arguments (between zero and [`SQLITE_MAX_FUNCTION_ARG`][]). If `false`,
    `function` must be invoked with exactly `function.length` arguments.
    **Default:** `false`.
* `function` {Function} The JavaScript function to call when the SQLite
  function is invoked. The return value of this function should be a valid
  SQLite data type: see [Type conversion between JavaScript and SQLite][].
  The result defaults to `NULL` if the return value is `undefined`.

This method is used to create SQLite user-defined functions. This method is a
wrapper around [`sqlite3_create_function_v2()`][].

### `database.setAuthorizer(callback)`

<!-- YAML
added: v24.10.0
-->

* `callback` {Function|null} The authorizer function to set, or `null` to
  clear the current authorizer.

Sets an authorizer callback that SQLite will invoke whenever it attempts to
access data or modify the database schema through prepared statements.
This can be used to implement security policies, audit access, or restrict certain operations.
This method is a wrapper around [`sqlite3_set_authorizer()`][].

When invoked, the callback receives five arguments:

* `actionCode` {number} The type of operation being performed (e.g.,
  `SQLITE_INSERT`, `SQLITE_UPDATE`, `SQLITE_SELECT`).
* `arg1` {string|null} The first argument (context-dependent, often a table name).
* `arg2` {string|null} The second argument (context-dependent, often a column name).
* `dbName` {string|null} The name of the database.
* `triggerOrView` {string|null} The name of the trigger or view causing the access.

The callback must return one of the following constants:

* `SQLITE_OK` - Allow the operation.
* `SQLITE_DENY` - Deny the operation (causes an error).
* `SQLITE_IGNORE` - Ignore the operation (silently skip).

```cjs
const { DatabaseSync, constants } = require('node:sqlite');
const db = new DatabaseSync(':memory:');

// Set up an authorizer that denies all table creation
db.setAuthorizer((actionCode) => {
  if (actionCode === constants.SQLITE_CREATE_TABLE) {
    return constants.SQLITE_DENY;
  }
  return constants.SQLITE_OK;
});

// This will work
db.prepare('SELECT 1').get();

// This will throw an error due to authorization denial
try {
  db.exec('CREATE TABLE blocked (id INTEGER)');
} catch (err) {
  console.log('Operation blocked:', err.message);
}
```

```mjs
import { DatabaseSync, constants } from 'node:sqlite';
const db = new DatabaseSync(':memory:');

// Set up an authorizer that denies all table creation
db.setAuthorizer((actionCode) => {
  if (actionCode === constants.SQLITE_CREATE_TABLE) {
    return constants.SQLITE_DENY;
  }
  return constants.SQLITE_OK;
});

// This will work
db.prepare('SELECT 1').get();

// This will throw an error due to authorization denial
try {
  db.exec('CREATE TABLE blocked (id INTEGER)');
} catch (err) {
  console.log('Operation blocked:', err.message);
}
```

### `database.isOpen`

<!-- YAML
added:
  - v23.11.0
  - v22.15.0
-->

* Type: {boolean} Whether the database is currently open or not.

### `database.isTransaction`

<!-- YAML
added:
  - v24.0.0
  - v22.16.0
-->

* Type: {boolean} Whether the database is currently within a transaction. This method
  is a wrapper around [`sqlite3_get_autocommit()`][].

### `database.open()`

<!-- YAML
added: v22.5.0
-->

Opens the database specified in the `path` argument of the `DatabaseSync`
constructor. This method should only be used when the database is not opened via
the constructor. An exception is thrown if the database is already open.

### `database.prepare(sql)`

<!-- YAML
added: v22.5.0
-->

* `sql` {string} A SQL string to compile to a prepared statement.
* Returns: {StatementSync} The prepared statement.

Compiles a SQL statement into a [prepared statement][]. This method is a wrapper
around [`sqlite3_prepare_v2()`][].

### `database.createTagStore([maxSize])`

<!-- YAML
added: v24.9.0
-->

* `maxSize` {integer} The maximum number of prepared statements to cache.
  **Default:** `1000`.
* Returns: {SQLTagStore} A new SQL tag store for caching prepared statements.

Creates a new [`SQLTagStore`][], which is a Least Recently Used (LRU) cache
for storing prepared statements. This allows for the efficient reuse of
prepared statements by tagging them with a unique identifier.

When a tagged SQL literal is executed, the `SQLTagStore` checks if a prepared
statement for the corresponding SQL query string already exists in the cache.
If it does, the cached statement is used. If not, a new prepared statement is
created, executed, and then stored in the cache for future use. This mechanism
helps to avoid the overhead of repeatedly parsing and preparing the same SQL
statements.

Tagged statements bind the placeholder values from the template literal as
parameters to the underlying prepared statement. For example:

```js
sqlTagStore.get`SELECT ${value}`;
```

is equivalent to:

```js
db.prepare('SELECT ?').get(value);
```

However, in the first example, the tag store will cache the underlying prepared
statement for future use.

> **Note:** The `${value}` syntax in tagged statements _binds_ a parameter to
> the prepared statement. This differs from its behavior in _untagged_ template
> literals, where it performs string interpolation.
>
> ```js
> // This a safe example of binding a parameter to a tagged statement.
> sqlTagStore.run`INSERT INTO t1 (id) VALUES (${id})`;
>
> // This is an *unsafe* example of an untagged template string.
> // `id` is interpolated into the query text as a string.
> // This can lead to SQL injection and data corruption.
> db.run(`INSERT INTO t1 (id) VALUES (${id})`);
> ```

The tag store will match a statement from the cache if the query strings
(including the positions of any bound placeholders) are identical.

```js
// The following statements will match in the cache:
sqlTagStore.get`SELECT * FROM t1 WHERE id = ${id} AND active = 1`;
sqlTagStore.get`SELECT * FROM t1 WHERE id = ${12345} AND active = 1`;

// The following statements will not match, as the query strings
// and bound placeholders differ:
sqlTagStore.get`SELECT * FROM t1 WHERE id = ${id} AND active = 1`;
sqlTagStore.get`SELECT * FROM t1 WHERE id = 12345 AND active = 1`;

// The following statements will not match, as matches are case-sensitive:
sqlTagStore.get`SELECT * FROM t1 WHERE id = ${id} AND active = 1`;
sqlTagStore.get`select * from t1 where id = ${id} and active = 1`;
```

The only way of binding parameters in tagged statements is with the `${value}`
syntax. Do not add parameter binding placeholders (`?` etc.) to the SQL query
string itself.

```mjs
import { DatabaseSync } from 'node:sqlite';

const db = new DatabaseSync(':memory:');
const sql = db.createTagStore();

db.exec('CREATE TABLE users (id INT, name TEXT)');

// Using the 'run' method to insert data.
// The tagged literal is used to identify the prepared statement.
sql.run`INSERT INTO users VALUES (1, 'Alice')`;
sql.run`INSERT INTO users VALUES (2, 'Bob')`;

// Using the 'get' method to retrieve a single row.
const name = 'Alice';
const user = sql.get`SELECT * FROM users WHERE name = ${name}`;
console.log(user); // { id: 1, name: 'Alice' }

// Using the 'all' method to retrieve all rows.
const allUsers = sql.all`SELECT * FROM users ORDER BY id`;
console.log(allUsers);
// [
//   { id: 1, name: 'Alice' },
//   { id: 2, name: 'Bob' }
// ]
```

```cjs
const { DatabaseSync } = require('node:sqlite');

const db = new DatabaseSync(':memory:');
const sql = db.createTagStore();

db.exec('CREATE TABLE users (id INT, name TEXT)');

// Using the 'run' method to insert data.
// The tagged literal is used to identify the prepared statement.
sql.run`INSERT INTO users VALUES (1, 'Alice')`;
sql.run`INSERT INTO users VALUES (2, 'Bob')`;

// Using the 'get' method to retrieve a single row.
const name = 'Alice';
const user = sql.get`SELECT * FROM users WHERE name = ${name}`;
console.log(user); // { id: 1, name: 'Alice' }

// Using the 'all' method to retrieve all rows.
const allUsers = sql.all`SELECT * FROM users ORDER BY id`;
console.log(allUsers);
// [
//   { id: 1, name: 'Alice' },
//   { id: 2, name: 'Bob' }
// ]
```

### `database.createSession([options])`

<!-- YAML
added:
  - v23.3.0
  - v22.12.0
-->

* `options` {Object} The configuration options for the session.
  * `table` {string} A specific table to track changes for. By default, changes to all tables are tracked.
  * `db` {string} Name of the database to track. This is useful when multiple databases have been added using [`ATTACH DATABASE`][]. **Default**: `'main'`.
* Returns: {Session} A session handle.

Creates and attaches a session to the database. This method is a wrapper around [`sqlite3session_create()`][] and [`sqlite3session_attach()`][].

### `database.applyChangeset(changeset[, options])`

<!-- YAML
added:
  - v23.3.0
  - v22.12.0
-->

* `changeset` {Uint8Array} A binary changeset or patchset.
* `options` {Object} The configuration options for how the changes will be applied.
  * `filter` {Function} Skip changes that, when targeted table name is supplied to this function, return a truthy value.
    By default, all changes are attempted.
  * `onConflict` {Function} A function that determines how to handle conflicts. The function receives one argument,
    which can be one of the following values:

    * `SQLITE_CHANGESET_DATA`: A `DELETE` or `UPDATE` change does not contain the expected "before" values.
    * `SQLITE_CHANGESET_NOTFOUND`: A row matching the primary key of the `DELETE` or `UPDATE` change does not exist.
    * `SQLITE_CHANGESET_CONFLICT`: An `INSERT` change results in a duplicate primary key.
    * `SQLITE_CHANGESET_FOREIGN_KEY`: Applying a change would result in a foreign key violation.
    * `SQLITE_CHANGESET_CONSTRAINT`: Applying a change results in a `UNIQUE`, `CHECK`, or `NOT NULL` constraint
      violation.

    The function should return one of the following values:

    * `SQLITE_CHANGESET_OMIT`: Omit conflicting changes.
    * `SQLITE_CHANGESET_REPLACE`: Replace existing values with conflicting changes (only valid with
      `SQLITE_CHANGESET_DATA` or `SQLITE_CHANGESET_CONFLICT` conflicts).
    * `SQLITE_CHANGESET_ABORT`: Abort on conflict and roll back the database.

    When an error is thrown in the conflict handler or when any other value is returned from the handler,
    applying the changeset is aborted and the database is rolled back.

    **Default**: A function that returns `SQLITE_CHANGESET_ABORT`.
* Returns: {boolean} Whether the changeset was applied successfully without being aborted.

An exception is thrown if the database is not
open. This method is a wrapper around [`sqlite3changeset_apply()`][].

```mjs
import { DatabaseSync } from 'node:sqlite';

const sourceDb = new DatabaseSync(':memory:');
const targetDb = new DatabaseSync(':memory:');

sourceDb.exec('CREATE TABLE data(key INTEGER PRIMARY KEY, value TEXT)');
targetDb.exec('CREATE TABLE data(key INTEGER PRIMARY KEY, value TEXT)');

const session = sourceDb.createSession();

const insert = sourceDb.prepare('INSERT INTO data (key, value) VALUES (?, ?)');
insert.run(1, 'hello');
insert.run(2, 'world');

const changeset = session.changeset();
targetDb.applyChangeset(changeset);
// Now that the changeset has been applied, targetDb contains the same data as sourceDb.
```

```cjs
const { DatabaseSync } = require('node:sqlite');

const sourceDb = new DatabaseSync(':memory:');
const targetDb = new DatabaseSync(':memory:');

sourceDb.exec('CREATE TABLE data(key INTEGER PRIMARY KEY, value TEXT)');
targetDb.exec('CREATE TABLE data(key INTEGER PRIMARY KEY, value TEXT)');

const session = sourceDb.createSession();

const insert = sourceDb.prepare('INSERT INTO data (key, value) VALUES (?, ?)');
insert.run(1, 'hello');
insert.run(2, 'world');

const changeset = session.changeset();
targetDb.applyChangeset(changeset);
// Now that the changeset has been applied, targetDb contains the same data as sourceDb.
```

### `database[Symbol.dispose]()`

<!-- YAML
added:
  - v23.11.0
  - v22.15.0
changes:
 - version: v24.2.0
   pr-url: https://github.com/nodejs/node/pull/58467
   description: No longer experimental.
-->

Closes the database connection. If the database connection is already closed
then this is a no-op.

## Class: `Session`

<!-- YAML
added:
  - v23.3.0
  - v22.12.0
-->

### `session.changeset()`

<!-- YAML
added:
  - v23.3.0
  - v22.12.0
-->

* Returns: {Uint8Array} Binary changeset that can be applied to other databases.

Retrieves a changeset containing all changes since the changeset was created. Can be called multiple times.
An exception is thrown if the database or the session is not open. This method is a wrapper around [`sqlite3session_changeset()`][].

### `session.patchset()`

<!-- YAML
added:
  - v23.3.0
  - v22.12.0
-->

* Returns: {Uint8Array} Binary patchset that can be applied to other databases.

Similar to the method above, but generates a more compact patchset. See [Changesets and Patchsets][]
in the documentation of SQLite. An exception is thrown if the database or the session is not open. This method is a
wrapper around [`sqlite3session_patchset()`][].

### `session.close()`.

Closes the session. An exception is thrown if the database or the session is not open. This method is a
wrapper around [`sqlite3session_delete()`][].

## Class: `StatementSync`

<!-- YAML
added: v22.5.0
-->

This class represents a single [prepared statement][]. This class cannot be
instantiated via its constructor. Instead, instances are created via the
`database.prepare()` method. All APIs exposed by this class execute
synchronously.

A prepared statement is an efficient binary representation of the SQL used to
create it. Prepared statements are parameterizable, and can be invoked multiple
times with different bound values. Parameters also offer protection against
[SQL injection][] attacks. For these reasons, prepared statements are preferred
over hand-crafted SQL strings when handling user input.

### `statement.all([namedParameters][, ...anonymousParameters])`

<!-- YAML
added: v22.5.0
changes:
  - version:
    - v23.7.0
    - v22.14.0
    pr-url: https://github.com/nodejs/node/pull/56385
    description: Add support for `DataView` and typed array objects for `anonymousParameters`.
-->

* `namedParameters` {Object} An optional object used to bind named parameters.
  The keys of this object are used to configure the mapping.
* `...anonymousParameters` {null|number|bigint|string|Buffer|TypedArray|DataView} Zero or
  more values to bind to anonymous parameters.
* Returns: {Array} An array of objects. Each object corresponds to a row
  returned by executing the prepared statement. The keys and values of each
  object correspond to the column names and values of the row.

This method executes a prepared statement and returns all results as an array of
objects. If the prepared statement does not return any results, this method
returns an empty array. The prepared statement [parameters are bound][] using
the values in `namedParameters` and `anonymousParameters`.

### `statement.columns()`

<!-- YAML
added:
  - v23.11.0
  - v22.16.0
-->

* Returns: {Array} An array of objects. Each object corresponds to a column
  in the prepared statement, and contains the following properties:

  * `column` {string|null} The unaliased name of the column in the origin
    table, or `null` if the column is the result of an expression or subquery.
    This property is the result of [`sqlite3_column_origin_name()`][].
  * `database` {string|null} The unaliased name of the origin database, or
    `null` if the column is the result of an expression or subquery. This
    property is the result of [`sqlite3_column_database_name()`][].
  * `name` {string} The name assigned to the column in the result set of a
    `SELECT` statement. This property is the result of
    [`sqlite3_column_name()`][].
  * `table` {string|null} The unaliased name of the origin table, or `null` if
    the column is the result of an expression or subquery. This property is the
    result of [`sqlite3_column_table_name()`][].
  * `type` {string|null} The declared data type of the column, or `null` if the
    column is the result of an expression or subquery. This property is the
    result of [`sqlite3_column_decltype()`][].

This method is used to retrieve information about the columns returned by the
prepared statement.

### `statement.expandedSQL`

<!-- YAML
added: v22.5.0
-->

* Type: {string} The source SQL expanded to include parameter values.

The source SQL text of the prepared statement with parameter
placeholders replaced by the values that were used during the most recent
execution of this prepared statement. This property is a wrapper around
[`sqlite3_expanded_sql()`][].

### `statement.get([namedParameters][, ...anonymousParameters])`

<!-- YAML
added: v22.5.0
changes:
  - version:
    - v23.7.0
    - v22.14.0
    pr-url: https://github.com/nodejs/node/pull/56385
    description: Add support for `DataView` and typed array objects for `anonymousParameters`.
-->

* `namedParameters` {Object} An optional object used to bind named parameters.
  The keys of this object are used to configure the mapping.
* `...anonymousParameters` {null|number|bigint|string|Buffer|TypedArray|DataView} Zero or
  more values to bind to anonymous parameters.
* Returns: {Object|undefined} An object corresponding to the first row returned
  by executing the prepared statement. The keys and values of the object
  correspond to the column names and values of the row. If no rows were returned
  from the database then this method returns `undefined`.

This method executes a prepared statement and returns the first result as an
object. If the prepared statement does not return any results, this method
returns `undefined`. The prepared statement [parameters are bound][] using the
values in `namedParameters` and `anonymousParameters`.

### `statement.iterate([namedParameters][, ...anonymousParameters])`

<!-- YAML
added:
  - v23.4.0
  - v22.13.0
changes:
  - version:
    - v23.7.0
    - v22.14.0
    pr-url: https://github.com/nodejs/node/pull/56385
    description: Add support for `DataView` and typed array objects for `anonymousParameters`.
-->

* `namedParameters` {Object} An optional object used to bind named parameters.
  The keys of this object are used to configure the mapping.
* `...anonymousParameters` {null|number|bigint|string|Buffer|TypedArray|DataView} Zero or
  more values to bind to anonymous parameters.
* Returns: {Iterator} An iterable iterator of objects. Each object corresponds to a row
  returned by executing the prepared statement. The keys and values of each
  object correspond to the column names and values of the row.

This method executes a prepared statement and returns an iterator of
objects. If the prepared statement does not return any results, this method
returns an empty iterator. The prepared statement [parameters are bound][] using
the values in `namedParameters` and `anonymousParameters`.

### `statement.run([namedParameters][, ...anonymousParameters])`

<!-- YAML
added: v22.5.0
changes:
  - version:
    - v23.7.0
    - v22.14.0
    pr-url: https://github.com/nodejs/node/pull/56385
    description: Add support for `DataView` and typed array objects for `anonymousParameters`.
-->

* `namedParameters` {Object} An optional object used to bind named parameters.
  The keys of this object are used to configure the mapping.
* `...anonymousParameters` {null|number|bigint|string|Buffer|TypedArray|DataView} Zero or
  more values to bind to anonymous parameters.
* Returns: {Object}
  * `changes` {number|bigint} The number of rows modified, inserted, or deleted
    by the most recently completed `INSERT`, `UPDATE`, or `DELETE` statement.
    This field is either a number or a `BigInt` depending on the prepared
    statement's configuration. This property is the result of
    [`sqlite3_changes64()`][].
  * `lastInsertRowid` {number|bigint} The most recently inserted rowid. This
    field is either a number or a `BigInt` depending on the prepared statement's
    configuration. This property is the result of
    [`sqlite3_last_insert_rowid()`][].

This method executes a prepared statement and returns an object summarizing the
resulting changes. The prepared statement [parameters are bound][] using the
values in `namedParameters` and `anonymousParameters`.

### `statement.setAllowBareNamedParameters(enabled)`

<!-- YAML
added: v22.5.0
-->

* `enabled` {boolean} Enables or disables support for binding named parameters
  without the prefix character.

The names of SQLite parameters begin with a prefix character. By default,
`node:sqlite` requires that this prefix character is present when binding
parameters. However, with the exception of dollar sign character, these
prefix characters also require extra quoting when used in object keys.

To improve ergonomics, this method can be used to also allow bare named
parameters, which do not require the prefix character in JavaScript code. There
are several caveats to be aware of when enabling bare named parameters:

* The prefix character is still required in SQL.
* The prefix character is still allowed in JavaScript. In fact, prefixed names
  will have slightly better binding performance.
* Using ambiguous named parameters, such as `$k` and `@k`, in the same prepared
  statement will result in an exception as it cannot be determined how to bind
  a bare name.

### `statement.setAllowUnknownNamedParameters(enabled)`

<!-- YAML
added:
  - v23.11.0
  - v22.15.0
-->

* `enabled` {boolean} Enables or disables support for unknown named parameters.

By default, if an unknown name is encountered while binding parameters, an
exception is thrown. This method allows unknown named parameters to be ignored.

### `statement.setReturnArrays(enabled)`

<!-- YAML
added:
  - v24.0.0
  - v22.16.0
-->

* `enabled` {boolean} Enables or disables the return of query results as arrays.

When enabled, query results returned by the `all()`, `get()`, and `iterate()` methods will be returned as arrays instead
of objects.

### `statement.setReadBigInts(enabled)`

<!-- YAML
added: v22.5.0
-->

* `enabled` {boolean} Enables or disables the use of `BigInt`s when reading
  `INTEGER` fields from the database.

When reading from the database, SQLite `INTEGER`s are mapped to JavaScript
numbers by default. However, SQLite `INTEGER`s can store values larger than
JavaScript numbers are capable of representing. In such cases, this method can
be used to read `INTEGER` data using JavaScript `BigInt`s. This method has no
impact on database write operations where numbers and `BigInt`s are both
supported at all times.

### `statement.sourceSQL`

<!-- YAML
added: v22.5.0
-->

* Type: {string} The source SQL used to create this prepared statement.

The source SQL text of the prepared statement. This property is a
wrapper around [`sqlite3_sql()`][].

## Class: `SQLTagStore`

<!-- YAML
added: v24.9.0
-->

This class represents a single LRU (Least Recently Used) cache for storing
prepared statements.

Instances of this class are created via the [`database.createTagStore()`][]
method, not by using a constructor. The store caches prepared statements based
on the provided SQL query string. When the same query is seen again, the store
retrieves the cached statement and safely applies the new values through
parameter binding, thereby preventing attacks like SQL injection.

The cache has a maxSize that defaults to 1000 statements, but a custom size can
be provided (e.g., `database.createTagStore(100)`). All APIs exposed by this
class execute synchronously.

### `sqlTagStore.all(stringElements[, ...boundParameters])`

<!-- YAML
added: v24.9.0
-->

* `stringElements` {string\[]} Template literal elements containing the SQL
  query.
* `...boundParameters` {null|number|bigint|string|Buffer|TypedArray|DataView}
  Parameter values to be bound to placeholders in the template string.
* Returns: {Array} An array of objects representing the rows returned by the query.

Executes the given SQL query and returns all resulting rows as an array of
objects.

This function is intended to be used as a template literal tag, not to be
called directly.

### `sqlTagStore.get(stringElements[, ...boundParameters])`

<!-- YAML
added: v24.9.0
-->

* `stringElements` {string\[]} Template literal elements containing the SQL
  query.
* `...boundParameters` {null|number|bigint|string|Buffer|TypedArray|DataView}
  Parameter values to be bound to placeholders in the template string.
* Returns: {Object | undefined} An object representing the first row returned by
  the query, or `undefined` if no rows are returned.

Executes the given SQL query and returns the first resulting row as an object.

This function is intended to be used as a template literal tag, not to be
called directly.

### `sqlTagStore.iterate(stringElements[, ...boundParameters])`

<!-- YAML
added: v24.9.0
-->

* `stringElements` {string\[]} Template literal elements containing the SQL
  query.
* `...boundParameters` {null|number|bigint|string|Buffer|TypedArray|DataView}
  Parameter values to be bound to placeholders in the template string.
* Returns: {Iterator} An iterator that yields objects representing the rows returned by the query.

Executes the given SQL query and returns an iterator over the resulting rows.

This function is intended to be used as a template literal tag, not to be
called directly.

### `sqlTagStore.run(stringElements[, ...boundParameters])`

<!-- YAML
added: v24.9.0
-->

* `stringElements` {string\[]} Template literal elements containing the SQL
  query.
* `...boundParameters` {null|number|bigint|string|Buffer|TypedArray|DataView}
  Parameter values to be bound to placeholders in the template string.
* Returns: {Object} An object containing information about the execution, including `changes` and `lastInsertRowid`.

Executes the given SQL query, which is expected to not return any rows (e.g., INSERT, UPDATE, DELETE).

This function is intended to be used as a template literal tag, not to be
called directly.

### `sqlTagStore.size`

<!-- YAML
added: v24.9.0
changes:
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/60246
    description: Changed from a method to a getter.
-->

* Type: {integer}

A read-only property that returns the number of prepared statements currently in the cache.

### `sqlTagStore.capacity`

<!-- YAML
added: v24.9.0
-->

* Type: {integer}

A read-only property that returns the maximum number of prepared statements the cache can hold.

### `sqlTagStore.db`

<!-- YAML
added: v24.9.0
-->

* Type: {DatabaseSync}

A read-only property that returns the `DatabaseSync` object associated with this `SQLTagStore`.

### `sqlTagStore.clear()`

<!-- YAML
added: v24.9.0
-->

Resets the LRU cache, clearing all stored prepared statements.

### Type conversion between JavaScript and SQLite

When Node.js writes to or reads from SQLite it is necessary to convert between
JavaScript data types and SQLite's [data types][]. Because JavaScript supports
more data types than SQLite, only a subset of JavaScript types are supported.
Attempting to write an unsupported data type to SQLite will result in an
exception.

| SQLite    | JavaScript                 |
| --------- | -------------------------- |
| `NULL`    | {null}                     |
| `INTEGER` | {number} or {bigint}       |
| `REAL`    | {number}                   |
| `TEXT`    | {string}                   |
| `BLOB`    | {TypedArray} or {DataView} |

## `sqlite.backup(sourceDb, path[, options])`

<!-- YAML
added:
  - v23.8.0
  - v22.16.0
changes:
  - version: v23.10.0
    pr-url: https://github.com/nodejs/node/pull/56991
    description: The `path` argument now supports Buffer and URL objects.
-->

* `sourceDb` {DatabaseSync} The database to backup. The source database must be open.
* `path` {string | Buffer | URL} The path where the backup will be created. If the file already exists,
  the contents will be overwritten.
* `options` {Object} Optional configuration for the backup. The
  following properties are supported:
  * `source` {string} Name of the source database. This can be `'main'` (the default primary database) or any other
    database that have been added with [`ATTACH DATABASE`][] **Default:** `'main'`.
  * `target` {string} Name of the target database. This can be `'main'` (the default primary database) or any other
    database that have been added with [`ATTACH DATABASE`][] **Default:** `'main'`.
  * `rate` {number} Number of pages to be transmitted in each batch of the backup. **Default:** `100`.
  * `progress` {Function} An optional callback function that will be called after each backup step. The argument passed
    to this callback is an {Object} with `remainingPages` and `totalPages` properties, describing the current progress
    of the backup operation.
* Returns: {Promise} A promise that fulfills with the total number of backed-up pages upon completion, or rejects if an
  error occurs.

This method makes a database backup. This method abstracts the [`sqlite3_backup_init()`][], [`sqlite3_backup_step()`][]
and [`sqlite3_backup_finish()`][] functions.

The backed-up database can be used normally during the backup process. Mutations coming from the same connection - same
{DatabaseSync} - object will be reflected in the backup right away. However, mutations from other connections will cause
the backup process to restart.

```cjs
const { backup, DatabaseSync } = require('node:sqlite');

(async () => {
  const sourceDb = new DatabaseSync('source.db');
  const totalPagesTransferred = await backup(sourceDb, 'backup.db', {
    rate: 1, // Copy one page at a time.
    progress: ({ totalPages, remainingPages }) => {
      console.log('Backup in progress', { totalPages, remainingPages });
    },
  });

  console.log('Backup completed', totalPagesTransferred);
})();
```

```mjs
import { backup, DatabaseSync } from 'node:sqlite';

const sourceDb = new DatabaseSync('source.db');
const totalPagesTransferred = await backup(sourceDb, 'backup.db', {
  rate: 1, // Copy one page at a time.
  progress: ({ totalPages, remainingPages }) => {
    console.log('Backup in progress', { totalPages, remainingPages });
  },
});

console.log('Backup completed', totalPagesTransferred);
```

## `sqlite.constants`

<!-- YAML
added:
  - v23.5.0
  - v22.13.0
-->

* Type: {Object}

An object containing commonly used constants for SQLite operations.

### SQLite constants

The following constants are exported by the `sqlite.constants` object.

#### Conflict resolution constants

One of the following constants is available as an argument to the `onConflict`
conflict resolution handler passed to [`database.applyChangeset()`][]. See also
[Constants Passed To The Conflict Handler][] in the SQLite documentation.

<table>
  <tr>
    <th>Constant</th>
    <th>Description</th>
  </tr>
  <tr>
    <td><code>SQLITE_CHANGESET_DATA</code></td>
    <td>The conflict handler is invoked with this constant when processing a DELETE or UPDATE change if a row with the required PRIMARY KEY fields is present in the database, but one or more other (non primary-key) fields modified by the update do not contain the expected "before" values.</td>
  </tr>
  <tr>
    <td><code>SQLITE_CHANGESET_NOTFOUND</code></td>
    <td>The conflict handler is invoked with this constant when processing a DELETE or UPDATE change if a row with the required PRIMARY KEY fields is not present in the database.</td>
  </tr>
  <tr>
    <td><code>SQLITE_CHANGESET_CONFLICT</code></td>
    <td>This constant is passed to the conflict handler while processing an INSERT change if the operation would result in duplicate primary key values.</td>
  </tr>
  <tr>
    <td><code>SQLITE_CHANGESET_CONSTRAINT</code></td>
    <td>If foreign key handling is enabled, and applying a changeset leaves the database in a state containing foreign key violations, the conflict handler is invoked with this constant exactly once before the changeset is committed. If the conflict handler returns <code>SQLITE_CHANGESET_OMIT</code>, the changes, including those that caused the foreign key constraint violation, are committed. Or, if it returns <code>SQLITE_CHANGESET_ABORT</code>, the changeset is rolled back.</td>
  </tr>
  <tr>
    <td><code>SQLITE_CHANGESET_FOREIGN_KEY</code></td>
    <td>If any other constraint violation occurs while applying a change (i.e. a UNIQUE, CHECK or NOT NULL constraint), the conflict handler is invoked with this constant.</td>
  </tr>
</table>

One of the following constants must be returned from the `onConflict` conflict
resolution handler passed to [`database.applyChangeset()`][]. See also
[Constants Returned From The Conflict Handler][] in the SQLite documentation.

<table>
  <tr>
    <th>Constant</th>
    <th>Description</th>
  </tr>
  <tr>
    <td><code>SQLITE_CHANGESET_OMIT</code></td>
    <td>Conflicting changes are omitted.</td>
  </tr>
  <tr>
    <td><code>SQLITE_CHANGESET_REPLACE</code></td>
    <td>Conflicting changes replace existing values. Note that this value can only be returned when the type of conflict is either <code>SQLITE_CHANGESET_DATA</code> or <code>SQLITE_CHANGESET_CONFLICT</code>.</td>
  </tr>
  <tr>
    <td><code>SQLITE_CHANGESET_ABORT</code></td>
    <td>Abort when a change encounters a conflict and roll back database.</td>
  </tr>
</table>

#### Authorization constants

The following constants are used with the [`database.setAuthorizer()`][] method.

##### Authorization result codes

One of the following constants must be returned from the authorizer callback
function passed to [`database.setAuthorizer()`][].

<table>
  <tr>
    <th>Constant</th>
    <th>Description</th>
  </tr>
  <tr>
    <td><code>SQLITE_OK</code></td>
    <td>Allow the operation to proceed normally.</td>
  </tr>
  <tr>
    <td><code>SQLITE_DENY</code></td>
    <td>Deny the operation and cause an error to be returned.</td>
  </tr>
  <tr>
    <td><code>SQLITE_IGNORE</code></td>
    <td>Ignore the operation and continue as if it had never been requested.</td>
  </tr>
</table>

##### Authorization action codes

The following constants are passed as the first argument to the authorizer
callback function to indicate what type of operation is being authorized.

<table>
  <tr>
    <th>Constant</th>
    <th>Description</th>
  </tr>
  <tr>
    <td><code>SQLITE_CREATE_INDEX</code></td>
    <td>Create an index</td>
  </tr>
  <tr>
    <td><code>SQLITE_CREATE_TABLE</code></td>
    <td>Create a table</td>
  </tr>
  <tr>
    <td><code>SQLITE_CREATE_TEMP_INDEX</code></td>
    <td>Create a temporary index</td>
  </tr>
  <tr>
    <td><code>SQLITE_CREATE_TEMP_TABLE</code></td>
    <td>Create a temporary table</td>
  </tr>
  <tr>
    <td><code>SQLITE_CREATE_TEMP_TRIGGER</code></td>
    <td>Create a temporary trigger</td>
  </tr>
  <tr>
    <td><code>SQLITE_CREATE_TEMP_VIEW</code></td>
    <td>Create a temporary view</td>
  </tr>
  <tr>
    <td><code>SQLITE_CREATE_TRIGGER</code></td>
    <td>Create a trigger</td>
  </tr>
  <tr>
    <td><code>SQLITE_CREATE_VIEW</code></td>
    <td>Create a view</td>
  </tr>
  <tr>
    <td><code>SQLITE_DELETE</code></td>
    <td>Delete from a table</td>
  </tr>
  <tr>
    <td><code>SQLITE_DROP_INDEX</code></td>
    <td>Drop an index</td>
  </tr>
  <tr>
    <td><code>SQLITE_DROP_TABLE</code></td>
    <td>Drop a table</td>
  </tr>
  <tr>
    <td><code>SQLITE_DROP_TEMP_INDEX</code></td>
    <td>Drop a temporary index</td>
  </tr>
  <tr>
    <td><code>SQLITE_DROP_TEMP_TABLE</code></td>
    <td>Drop a temporary table</td>
  </tr>
  <tr>
    <td><code>SQLITE_DROP_TEMP_TRIGGER</code></td>
    <td>Drop a temporary trigger</td>
  </tr>
  <tr>
    <td><code>SQLITE_DROP_TEMP_VIEW</code></td>
    <td>Drop a temporary view</td>
  </tr>
  <tr>
    <td><code>SQLITE_DROP_TRIGGER</code></td>
    <td>Drop a trigger</td>
  </tr>
  <tr>
    <td><code>SQLITE_DROP_VIEW</code></td>
    <td>Drop a view</td>
  </tr>
  <tr>
    <td><code>SQLITE_INSERT</code></td>
    <td>Insert into a table</td>
  </tr>
  <tr>
    <td><code>SQLITE_PRAGMA</code></td>
    <td>Execute a PRAGMA statement</td>
  </tr>
  <tr>
    <td><code>SQLITE_READ</code></td>
    <td>Read from a table</td>
  </tr>
  <tr>
    <td><code>SQLITE_SELECT</code></td>
    <td>Execute a SELECT statement</td>
  </tr>
  <tr>
    <td><code>SQLITE_TRANSACTION</code></td>
    <td>Begin, commit, or rollback a transaction</td>
  </tr>
  <tr>
    <td><code>SQLITE_UPDATE</code></td>
    <td>Update a table</td>
  </tr>
  <tr>
    <td><code>SQLITE_ATTACH</code></td>
    <td>Attach a database</td>
  </tr>
  <tr>
    <td><code>SQLITE_DETACH</code></td>
    <td>Detach a database</td>
  </tr>
  <tr>
    <td><code>SQLITE_ALTER_TABLE</code></td>
    <td>Alter a table</td>
  </tr>
  <tr>
    <td><code>SQLITE_REINDEX</code></td>
    <td>Reindex</td>
  </tr>
  <tr>
    <td><code>SQLITE_ANALYZE</code></td>
    <td>Analyze the database</td>
  </tr>
  <tr>
    <td><code>SQLITE_CREATE_VTABLE</code></td>
    <td>Create a virtual table</td>
  </tr>
  <tr>
    <td><code>SQLITE_DROP_VTABLE</code></td>
    <td>Drop a virtual table</td>
  </tr>
  <tr>
    <td><code>SQLITE_FUNCTION</code></td>
    <td>Use a function</td>
  </tr>
  <tr>
    <td><code>SQLITE_SAVEPOINT</code></td>
    <td>Create, release, or rollback a savepoint</td>
  </tr>
  <tr>
    <td><code>SQLITE_COPY</code></td>
    <td>Copy data (legacy)</td>
  </tr>
  <tr>
    <td><code>SQLITE_RECURSIVE</code></td>
    <td>Recursive query</td>
  </tr>
</table>

[Changesets and Patchsets]: https://www.sqlite.org/sessionintro.html#changesets_and_patchsets
[Constants Passed To The Conflict Handler]: https://www.sqlite.org/session/c_changeset_conflict.html
[Constants Returned From The Conflict Handler]: https://www.sqlite.org/session/c_changeset_abort.html
[SQL injection]: https://en.wikipedia.org/wiki/SQL_injection
[Type conversion between JavaScript and SQLite]: #type-conversion-between-javascript-and-sqlite
[`ATTACH DATABASE`]: https://www.sqlite.org/lang_attach.html
[`PRAGMA foreign_keys`]: https://www.sqlite.org/pragma.html#pragma_foreign_keys
[`SQLITE_DBCONFIG_DEFENSIVE`]: https://www.sqlite.org/c3ref/c_dbconfig_defensive.html#sqlitedbconfigdefensive
[`SQLITE_DETERMINISTIC`]: https://www.sqlite.org/c3ref/c_deterministic.html
[`SQLITE_DIRECTONLY`]: https://www.sqlite.org/c3ref/c_deterministic.html
[`SQLITE_MAX_FUNCTION_ARG`]: https://www.sqlite.org/limits.html#max_function_arg
[`SQLTagStore`]: #class-sqltagstore
[`database.applyChangeset()`]: #databaseapplychangesetchangeset-options
[`database.createTagStore()`]: #databasecreatetagstoremaxsize
[`database.setAuthorizer()`]: #databasesetauthorizercallback
[`sqlite3_backup_finish()`]: https://www.sqlite.org/c3ref/backup_finish.html#sqlite3backupfinish
[`sqlite3_backup_init()`]: https://www.sqlite.org/c3ref/backup_finish.html#sqlite3backupinit
[`sqlite3_backup_step()`]: https://www.sqlite.org/c3ref/backup_finish.html#sqlite3backupstep
[`sqlite3_changes64()`]: https://www.sqlite.org/c3ref/changes.html
[`sqlite3_close_v2()`]: https://www.sqlite.org/c3ref/close.html
[`sqlite3_column_database_name()`]: https://www.sqlite.org/c3ref/column_database_name.html
[`sqlite3_column_decltype()`]: https://www.sqlite.org/c3ref/column_decltype.html
[`sqlite3_column_name()`]: https://www.sqlite.org/c3ref/column_name.html
[`sqlite3_column_origin_name()`]: https://www.sqlite.org/c3ref/column_database_name.html
[`sqlite3_column_table_name()`]: https://www.sqlite.org/c3ref/column_database_name.html
[`sqlite3_create_function_v2()`]: https://www.sqlite.org/c3ref/create_function.html
[`sqlite3_create_window_function()`]: https://www.sqlite.org/c3ref/create_function.html
[`sqlite3_db_filename()`]: https://sqlite.org/c3ref/db_filename.html
[`sqlite3_exec()`]: https://www.sqlite.org/c3ref/exec.html
[`sqlite3_expanded_sql()`]: https://www.sqlite.org/c3ref/expanded_sql.html
[`sqlite3_get_autocommit()`]: https://sqlite.org/c3ref/get_autocommit.html
[`sqlite3_last_insert_rowid()`]: https://www.sqlite.org/c3ref/last_insert_rowid.html
[`sqlite3_load_extension()`]: https://www.sqlite.org/c3ref/load_extension.html
[`sqlite3_prepare_v2()`]: https://www.sqlite.org/c3ref/prepare.html
[`sqlite3_set_authorizer()`]: https://sqlite.org/c3ref/set_authorizer.html
[`sqlite3_sql()`]: https://www.sqlite.org/c3ref/expanded_sql.html
[`sqlite3changeset_apply()`]: https://www.sqlite.org/session/sqlite3changeset_apply.html
[`sqlite3session_attach()`]: https://www.sqlite.org/session/sqlite3session_attach.html
[`sqlite3session_changeset()`]: https://www.sqlite.org/session/sqlite3session_changeset.html
[`sqlite3session_create()`]: https://www.sqlite.org/session/sqlite3session_create.html
[`sqlite3session_delete()`]: https://www.sqlite.org/session/sqlite3session_delete.html
[`sqlite3session_patchset()`]: https://www.sqlite.org/session/sqlite3session_patchset.html
[busy timeout]: https://sqlite.org/c3ref/busy_timeout.html
[connection]: https://www.sqlite.org/c3ref/sqlite3.html
[data types]: https://www.sqlite.org/datatype3.html
[double-quoted string literals]: https://www.sqlite.org/quirks.html#dblquote
[in memory]: https://www.sqlite.org/inmemorydb.html
[parameters are bound]: https://www.sqlite.org/c3ref/bind_blob.html
[prepared statement]: https://www.sqlite.org/c3ref/stmt.html
