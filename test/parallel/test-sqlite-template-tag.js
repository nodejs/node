const assert = require('assert');
const { sql } = require('node:sqlite');
const { test, suite } = require('node:test');

suite('sqlite template tag', () => {
    test('should correctly create an SqlTaggedTemplateQuery object with a string parameter', () => {
        const name = 'Ankan';
        const SqlTaggedTemplateQuery = sql`SELECT * FROM users WHERE name = ${name}`;

        assert.strictEqual(SqlTaggedTemplateQuery.sql, 'SELECT * FROM users WHERE name = ?');
        assert.deepStrictEqual(SqlTaggedTemplateQuery.params, [name]);
        assert.strictEqual(SqlTaggedTemplateQuery.toString(), `SELECT * FROM users WHERE name = '${name}'`);
    });

    test('should correctly create an SqlTaggedTemplateQuery object with a number parameter', () => {
        const age = 30;
        const SqlTaggedTemplateQuery = sql`SELECT * FROM users WHERE age = ${age}`;

        assert.strictEqual(SqlTaggedTemplateQuery.sql, 'SELECT * FROM users WHERE age = ?');
        assert.deepStrictEqual(SqlTaggedTemplateQuery.params, [age]);
        assert.strictEqual(SqlTaggedTemplateQuery.toString(), `SELECT * FROM users WHERE age = ${age}`);
    });

    test('should correctly create an SqlTaggedTemplateQuery object with a boolean true parameter', () => {
        const isActive = true;
        const SqlTaggedTemplateQuery = sql`SELECT * FROM users WHERE active = ${isActive}`;

        assert.strictEqual(SqlTaggedTemplateQuery.sql, 'SELECT * FROM users WHERE active = ?');
        assert.deepStrictEqual(SqlTaggedTemplateQuery.params, [isActive]);
        assert.strictEqual(SqlTaggedTemplateQuery.toString(), 'SELECT * FROM users WHERE active = 1');
    });

    test('should correctly create an SqlTaggedTemplateQuery object with a boolean false parameter', () => {
        const isDeleted = false;
        const SqlTaggedTemplateQuery = sql`SELECT * FROM users WHERE deleted = ${isDeleted}`;

        assert.strictEqual(SqlTaggedTemplateQuery.sql, 'SELECT * FROM users WHERE deleted = ?');
        assert.deepStrictEqual(SqlTaggedTemplateQuery.params, [isDeleted]);
        assert.strictEqual(SqlTaggedTemplateQuery.toString(), 'SELECT * FROM users WHERE deleted = 0');
    });

    test('should correctly create an SqlTaggedTemplateQuery object with a null parameter', () => {
        const middleName = null;
        const SqlTaggedTemplateQuery = sql`SELECT * FROM users WHERE middle_name = ${middleName}`;

        assert.strictEqual(SqlTaggedTemplateQuery.sql, 'SELECT * FROM users WHERE middle_name = ?');
        assert.deepStrictEqual(SqlTaggedTemplateQuery.params, [middleName]);
        assert.strictEqual(SqlTaggedTemplateQuery.toString(), 'SELECT * FROM users WHERE middle_name = NULL');
    });

    test('should correctly create an SqlTaggedTemplateQuery object with an undefined parameter', () => {
        const nickname = undefined;
        const SqlTaggedTemplateQuery = sql`SELECT * FROM users WHERE nickname = ${nickname}`;

        assert.strictEqual(SqlTaggedTemplateQuery.sql, 'SELECT * FROM users WHERE nickname = ?');
        assert.deepStrictEqual(SqlTaggedTemplateQuery.params, [nickname]);
        assert.strictEqual(SqlTaggedTemplateQuery.toString(), 'SELECT * FROM users WHERE nickname = NULL');
    });


    test('should correctly create an SqlTaggedTemplateQuery object with a BigInt parameter', () => {
        const id = 9007199254740991n;
        const sqlQuery = sql`SELECT * FROM users WHERE id = ${id}`;

        assert.strictEqual(sqlQuery.sql, 'SELECT * FROM users WHERE id = ?');
        assert.deepStrictEqual(sqlQuery.params, [id]);
        assert.strictEqual(sqlQuery.toString(), `SELECT * FROM users WHERE id = ${id}`);
    });

    test('should correctly create an SqlTaggedTemplateQuery object with a float parameter', () => {
        const price = 19.99;
        const sqlQuery = sql`SELECT * FROM products WHERE price = ${price}`;

        assert.strictEqual(sqlQuery.sql, 'SELECT * FROM products WHERE price = ?');
        assert.deepStrictEqual(sqlQuery.params, [price]);
        assert.strictEqual(sqlQuery.toString(), 'SELECT * FROM products WHERE price = 19.99');
    });

    test('should handle multiple parameters of different types', () => {
        const name = 'Ankan';
        const age = 30;
        const isActive = true;
        const sqlQuery = sql`SELECT * FROM users WHERE name = ${name} AND age > ${age} AND active = ${isActive}`;

        assert.strictEqual(sqlQuery.sql, 'SELECT * FROM users WHERE name = ? AND age > ? AND active = ?');
        assert.deepStrictEqual(sqlQuery.params, [name, age, isActive]);
        assert.strictEqual(sqlQuery.toString(), `SELECT * FROM users WHERE name = '${name}' AND age > ${age} AND active = 1`);
    });

    test('should handle strings with single quotes', () => {
        const company = "Ankan's Company";
        const sqlQuery = sql`INSERT INTO companies (name) VALUES (${company})`;

        assert.strictEqual(sqlQuery.sql, 'INSERT INTO companies (name) VALUES (?)');
        assert.deepStrictEqual(sqlQuery.params, [company]);
        assert.strictEqual(sqlQuery.toString(), "INSERT INTO companies (name) VALUES ('Ankan''s Company')");
    });

    test('should handle an empty template literal', () => {
        const sqlQuery = sql``;
        assert.strictEqual(sqlQuery.sql, '');
        assert.deepStrictEqual(sqlQuery.params, []);
        assert.strictEqual(sqlQuery.toString(), '');
    });

    test('should handle a template literal with no parameters', () => {
        const sqlQuery = sql`SELECT * FROM users`;
        assert.strictEqual(sqlQuery.sql, 'SELECT * FROM users');
        assert.deepStrictEqual(sqlQuery.params, []);
        assert.strictEqual(sqlQuery.toString(), 'SELECT * FROM users');
    });
});