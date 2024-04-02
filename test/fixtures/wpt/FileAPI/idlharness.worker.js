importScripts("/resources/testharness.js");
importScripts("/resources/WebIDLParser.js", "/resources/idlharness.js");

'use strict';

// https://w3c.github.io/FileAPI/

idl_test(
  ['FileAPI'],
  ['dom', 'html', 'url'],
  idl_array => {
    idl_array.add_objects({
      Blob: ['new Blob(["TEST"])'],
      File: ['new File(["myFileBits"], "myFileName")'],
      FileReader: ['new FileReader()'],
      FileReaderSync: ['new FileReaderSync()']
    });
  }
);
done();
