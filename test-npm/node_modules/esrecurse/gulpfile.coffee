# Copyright (C) 2014 Yusuke Suzuki <utatane.tea@gmail.com>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#   * Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#   * Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

gulp = require 'gulp'
mocha = require 'gulp-mocha'
eslint = require 'gulp-eslint'
minimist = require 'minimist'
git = require 'gulp-git'
bump = require 'gulp-bump'
filter = require 'gulp-filter'
tagVersion = require 'gulp-tag-version'
require 'coffee-script/register'

SOURCE = [
    '*.js'
]

ESLINT_OPTION =
    rules:
        'quotes': 0
        'eqeqeq': 0
        'no-use-before-define': 0
        'no-shadow': 0
        'no-new': 0
        'no-underscore-dangle': 0
        'no-multi-spaces': 0
        'no-native-reassign': 0
        'no-loop-func': 0
    env:
        'node': true

gulp.task 'test', ->
    options = minimist process.argv.slice(2),
        string: 'test',
        default:
            test: 'test/*.coffee'
    return gulp.src(options.test).pipe(mocha reporter: 'spec')

gulp.task 'lint', ->
    return gulp.src(SOURCE)
    .pipe(eslint(ESLINT_OPTION))
    .pipe(eslint.formatEach('stylish', process.stderr))
    .pipe(eslint.failOnError())

inc = (importance) ->
    gulp.src(['./package.json'])
        .pipe(bump({type: importance}))
        .pipe(gulp.dest('./'))
        .pipe(git.commit('Bumps package version'))
        .pipe(filter('package.json'))
        .pipe(tagVersion({
            prefix: ''
        }))

gulp.task 'travis', [ 'lint', 'test' ]
gulp.task 'default', [ 'travis' ]

gulp.task 'patch', [ ], -> inc('patch')
gulp.task 'minor', [ ], -> inc('minor')
gulp.task 'major', [ ], -> inc('major')
