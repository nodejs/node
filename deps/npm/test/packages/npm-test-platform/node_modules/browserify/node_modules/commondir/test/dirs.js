var assert = require('assert');
var commondir = require('../');

exports.common = function () {
    assert.equal(
        commondir([ '/foo', '//foo/bar', '/foo//bar/baz' ]),
        '/foo'
    );
    
    assert.equal(
        commondir([ '/a/b/c', '/a/b', '/a/b/c/d/e' ]),
        '/a/b'
    );
    
    assert.equal(
        commondir([ '/x/y/z/w', '/xy/z', '/x/y/z' ]),
        '/'
    );

    assert.equal(
        commondir([ 'X:\\foo', 'X:\\\\foo\\bar', 'X://foo/bar/baz' ]),
        'X:/foo'
    );
    
    assert.equal(
        commondir([ 'X:\\a\\b\\c', 'X:\\a\\b', 'X:\\a\\b\\c\\d\\e' ]),
        'X:/a/b'
    );
    
    assert.equal(
        commondir([ 'X:\\x\\y\\z\\w', '\\\\xy\\z', '\\x\\y\\z' ]),
        '/'
    );
    
    assert.throws(function () {
        assert.equal(
            commondir([ '/x/y/z/w', 'qrs', '/x/y/z' ]),
            '/'
        );
    });
};

exports.base = function () {
    assert.equal(
        commondir('/foo/bar', [ 'baz', './quux', '../bar/bazzy' ]),
        '/foo/bar'
    );
    
    assert.equal(
        commondir('/a/b', [ 'c', '../b/.', '../../a/b/e' ]),
        '/a/b'
    );
    
    assert.equal(
        commondir('/a/b/c', [ '..', '../d', '../../a/z/e' ]),
        '/a'
    );

    assert.equal(
        commondir('/foo/bar', [ 'baz', '.\\quux', '..\\bar\\bazzy' ]),
        '/foo/bar'
    );

    // Tests including X:\ basedirs must wait until path.resolve supports
    // Windows-style paths, starting in Node.js v0.5.X
};
