/*
  Copyright (C) 2014 Yusuke Suzuki <utatane.tea@gmail.com>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 'AS IS'
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

'use strict';

var gulp = require('gulp');
var mocha = require('gulp-mocha');
var jshint = require('gulp-jshint');
var eslint = require('gulp-eslint');
var istanbul = require('gulp-istanbul');
var bump = require('gulp-bump');
var filter = require('gulp-filter');
var git = require('gulp-git');
var tagVersion = require('gulp-tag-version');

var SRC = [ 'lib/*.js' ];

var TEST = [ 'test/*.js' ];

var LINT = [
    'gulpfile.js'
].concat(SRC);

var ESLINT_OPTION = {
    'rules': {
        'quotes': 0,
        'eqeqeq': 0,
        'no-use-before-define': 0,
        'no-underscore-dangle': 0,
        'no-shadow': 0,
        'no-constant-condition': 0,
        'no-multi-spaces': 0,
        'dot-notation': [2, {'allowKeywords': false}]
    },
    'env': {
        'node': true
    }
};

gulp.task('test', function (cb) {
    gulp.src(SRC)
        .pipe(istanbul()) // Covering files
        .on('finish', function () {
            gulp.src(TEST)
                .pipe(mocha({
                    reporter: 'spec',
                    timeout: 100000 // 100s
                    }))
                .pipe(istanbul.writeReports()) // Creating the reports after tests runned
                .on('end', cb);
        });
});

gulp.task('lint', function () {
    return gulp.src(LINT)
        .pipe(jshint('.jshintrc'))
        .pipe(jshint.reporter(require('jshint-stylish')))
        .pipe(jshint.reporter('fail'))
        .pipe(eslint(ESLINT_OPTION))
        .pipe(eslint.formatEach('compact', process.stderr))
        .pipe(eslint.failOnError());
});


/**
 * Bumping version number and tagging the repository with it.
 * Please read http://semver.org/
 *
 * You can use the commands
 *
 *     gulp patch     # makes v0.1.0 -> v0.1.1
 *     gulp feature   # makes v0.1.1 -> v0.2.0
 *     gulp release   # makes v0.2.1 -> v1.0.0
 *
 * To bump the version numbers accordingly after you did a patch,
 * introduced a feature or made a backwards-incompatible release.
 */

function inc(importance) {
    // get all the files to bump version in
    return gulp.src(['./package.json'])
        // bump the version number in those files
        .pipe(bump({type: importance}))
        // save it back to filesystem
        .pipe(gulp.dest('./'))
        // commit the changed version number
        .pipe(git.commit('Bumps package version'))
        // read only one file to get the version number
        .pipe(filter('package.json'))
        // **tag it in the repository**
        .pipe(tagVersion({ prefix: '' }));
}

gulp.task('patch', [ 'travis' ], function () { return inc('patch'); });
gulp.task('minor', [ 'travis' ], function () { return inc('minor'); });
gulp.task('major', [ 'travis' ], function () { return inc('major'); });

gulp.task('travis', [ 'lint', 'test' ]);
gulp.task('default', [ 'travis' ]);
