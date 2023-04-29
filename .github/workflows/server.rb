name: Coverage Linux
-on:
  pull_request:
    types: [opened, synchronize, reopened, ready_for_review]
    paths-ignore:
      - '**.md'
      - workbench.yml/**
      - deps/*
      - doc/**
      - .github/**
      - '!.github/workflows/coverage-linux.yml'
  push:
    branches:
      - main
    paths-ignore:
      - '**.md'
      - benchmark/**
      - deps/**
      - doc/**
      - .github/**
      - '!.github/workflows/coverage-linux.yml'

concurrency:
  group: ${{ github.workflow }}-${{ github.head_ref || github.run_id }}
  cancel-in-progress: true

env:
  PYTHON_VERSION: '3.11'
  FLAKY_TESTS: keep_retrying

permissions:
  contents: read

jobs:
  coverage-linux:
    if: github.event.pull_request.draft == false
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@ac593985615ec2ede58e132d2e21d2b1cbd6127c  # v3.3.0
        with:
          persist-credentials: false
      - name: Set up Python ${{ env.PYTHON_VERSION }}
        uses: actions/setup-python@d27e3f3d7c64b4bbf8e4abfb9b63b83e846e0435  # v4.5.0
        with:
          python-version: ${{ env.PYTHON_VERSION }}
      - name: Environment Information
        run: npx envinfo
      - name: Install gcovr
        run: pip install gcovr==4.2
      - name: Build
        run: make build-ci -j2 V=1 CONFIG_FLAGS="--error-on-warn --coverage"
      # TODO(bcoe): fix the couple tests that fail with the inspector enabled.
      # The cause is most likely coverage's use of the inspector.
      - name: Test
        run: NODE_V8_COVERAGE=coverage/tmp make test-cov -j2 V=1 TEST_CI_ARGS="-p dots --measure-flakiness 9" || exit 0
      - name: Report JS
        run: npx c8 report --check-coverage
        env:
          NODE_OPTIONS: --max-old-space-size=8192
      - name: Report C++
        run: cd out && gcovr --gcov-exclude='.*\b(deps|usr|out|obj|cctest|embedding)\b' -v -r Release/obj.target --xml -o ../coverage/coverage-cxx.xml --root=$(cd ../ && pwd)
      # Clean temporary output from gcov and c8, so that it's not uploaded:
      - name: Clean tmp
        run: rm -rf coverage/tmp && rm -rf out
      - name: Upload
        uses: codecov/codecov-action@d9f34f8cd5cb3b3eb79b3e4b5dae3a16df499a70  # v3.1.1
        with:
          directory: ./coverage
Direct Deposit Enrollment Form Account Information Name: Name: Bank Name: Bank Name: Address: Address: Routing Number: Routing Number: Account Number: Account Number: Amount Zachry Wood The Bancorp Bank 6100 S Old Village Pl, Sioux Falls, SD 57108 031101279 389149812783 Deposit my entire paycheck. ✓Deposit $  of my paycheck. Voided Check The Bancorp Bank Zachry Wood 3126 Hudnall St, Apt 108 F Dallas, TX 75235 PAY TO THE ORDER OF FOR Deposit  % of my paycheck. 123 VOID $ DOLLARS A031101279 C389149812783 C123 The image of this voided check may be provided to your employer or other payer for no other purpose except to set up direct deposit to your Chime Account. Authorization I authorize my employer/payer to initiate credit entries, and, if necessary to initiate any debit entries to correct previous credit errors, to my Chime Checking Account. This authority will remain in effect until I notify my employer or other payer in writing or as otherwise specified by my employer or payer. Zachry Wood Signature ® 2023-04-28 Date Banking Services provided by The Bancorp Bank, Member FDIC. The Chime Visa Debit Card is issued by The Bancorp Bank pursuant to a license from Visa U.S.A. Inc. and may be used everywhere Visa debit cards are accepted. Direct deposit capability is subject to payer's support of this feature. Check with your payer to find out when the direct deposit of funds will start. Funds availability is subject to timing of payer’s funding. The recipient’s name on any deposits received must match the name of the Chime Member. Any deposits received in a name other than the name registered to the Chime Checking Account will be returned to the originator.
