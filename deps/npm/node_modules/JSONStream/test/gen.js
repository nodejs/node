return // dont run this test for now since tape is weird and broken on 0.10

var fs = require('fs')
var JSONStream = require('../')
var file = process.argv[2] || '/tmp/JSONStream-test-large.json'
var size = Number(process.argv[3] || 100000)
var tape = require('tape')
// if (process.title !== 'browser') {
  tape('out of mem', function (t) {
    t.plan(1)
    //////////////////////////////////////////////////////
    // Produces a random number between arg1 and arg2
    //////////////////////////////////////////////////////
    var randomNumber = function (min, max) {
      var number = Math.floor(Math.random() * (max - min + 1) + min);
      return number;
    };

    //////////////////////////////////////////////////////
    // Produces a random string of a length between arg1 and arg2
    //////////////////////////////////////////////////////
    var randomString = function (min, max) {

      // add several spaces to increase chanses of creating 'words'
      var chars = '      0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ';
      var result = '';

      var randomLength = randomNumber(min, max);

      for (var i = randomLength; i > 0; --i) {
        result += chars[Math.round(Math.random() * (chars.length - 1))];
      }
      return result;
    };

    //////////////////////////////////////////////////////
    // Produces a random JSON document, as a string
    //////////////////////////////////////////////////////
    var randomJsonDoc = function () {

      var doc = {
        "CrashOccurenceID": randomNumber(10000, 50000),
        "CrashID": randomNumber(1000, 10000),
        "SiteName": randomString(10, 25),
        "MachineName": randomString(10, 25),
        "Date": randomString(26, 26),
        "ProcessDuration": randomString(18, 18),
        "ThreadIdentityName": null,
        "WindowsIdentityName": randomString(15, 40),
        "OperatingSystemName": randomString(35, 65),
        "DetailedExceptionInformation": randomString(100, 800)
      };

      doc = JSON.stringify(doc);
      doc = doc.replace(/\,/g, ',\n'); // add new lines after each attribute
      return doc;
    };

    //////////////////////////////////////////////////////
    // generates test data
    //////////////////////////////////////////////////////
    var generateTestData = function (cb) {

      console.log('generating large data file...');

      var stream = fs.createWriteStream(file, {
        encoding: 'utf8'
      });

      var i = 0;
      var max = size;
      var writing = false
      var split = ',\n';
      var doc = randomJsonDoc();
      stream.write('[');

      function write () {
        if(writing) return
        writing = true
        while(++i < max) {
          if(Math.random() < 0.001)
            console.log('generate..', i + ' / ' + size)
          if(!stream.write(doc + split)) {
            writing = false
            return stream.once('drain', write)
          }
        }
        stream.write(doc + ']')
        stream.end();
        console.log('END')
      }
      write()
      stream.on('close', cb)
    };

    //////////////////////////////////////////////////////
    // Shows that parsing 100000 instances using JSONStream fails
    //
    // After several seconds, you will get this crash
    //     FATAL ERROR: JS Allocation failed - process out of memory
    //////////////////////////////////////////////////////
    var testJSONStreamParse_causesOutOfMem = function (done) {
      var items = 0
      console.log('parsing data files using JSONStream...');

      var parser = JSONStream.parse([true]);
      var stream = fs.createReadStream(file);
      stream.pipe(parser);

      parser.on('data', function (data) {
        items++
        if(Math.random() < 0.01) console.log(items, '...')
      });

      parser.on('end', function () {
        t.equal(items, size)
      });

    };

    //////////////////////////////////////////////////////
    // main
    //////////////////////////////////////////////////////

    fs.stat(file, function (err, stat) {
      console.log(stat)
      if(err)
        generateTestData(testJSONStreamParse_causesOutOfMem);
      else
        testJSONStreamParse_causesOutOfMem()
    })

  })

// }
