name: CI

on:
  pull_request:
  push:
    branches:
      - master

jobs:
  build:
    runs-on: ubuntu-latest
    name: Ruby ${{ matrix.ruby }}
    strategy:
      matrix:
        ruby: ["2.7", "2.6", "2.5"]
    steps:
      - uses: actions/checkout@v2
      - uses: actions/setup-ruby@v1
        with:
          ruby-version: ${{ matrix.ruby }}
      - name: Bundler cache
        uses: actions/cache@v2
        with:
          path: vendor/bundle
          key: ${{ runner.os }}-${{ matrix.ruby }}-gems-${{ hashFiles('**/Gemfile.lock') }}
          restore-keys: |
            ${{ runner.os }}-${{ matrix.ruby }}-gems-
      - name: Setup gems
        run: |
          bundle config path vendor/bundle
          bundle install --jobs 4
      - name: Tests
        run: bundle exec rake test
