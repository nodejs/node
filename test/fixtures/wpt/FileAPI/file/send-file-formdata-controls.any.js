// META: title=FormData: FormData: Upload files named using controls
// META: script=../support/send-file-formdata-helper.js
  "use strict";

  formDataPostFileUploadTest({
    fileNameSource: "ASCII",
    fileBaseName: "file-for-upload-in-form-NUL-[\0].txt",
  });

  formDataPostFileUploadTest({
    fileNameSource: "ASCII",
    fileBaseName: "file-for-upload-in-form-BS-[\b].txt",
  });

  formDataPostFileUploadTest({
    fileNameSource: "ASCII",
    fileBaseName: "file-for-upload-in-form-VT-[\v].txt",
  });

  // These have characters that undergo processing in name=,
  // filename=, and/or value; formDataPostFileUploadTest postprocesses
  // expectedEncodedBaseName for these internally.

  formDataPostFileUploadTest({
    fileNameSource: "ASCII",
    fileBaseName: "file-for-upload-in-form-LF-[\n].txt",
  });

  formDataPostFileUploadTest({
    fileNameSource: "ASCII",
    fileBaseName: "file-for-upload-in-form-LF-CR-[\n\r].txt",
  });

  formDataPostFileUploadTest({
    fileNameSource: "ASCII",
    fileBaseName: "file-for-upload-in-form-CR-[\r].txt",
  });

  formDataPostFileUploadTest({
    fileNameSource: "ASCII",
    fileBaseName: "file-for-upload-in-form-CR-LF-[\r\n].txt",
  });

  formDataPostFileUploadTest({
    fileNameSource: "ASCII",
    fileBaseName: "file-for-upload-in-form-HT-[\t].txt",
  });

  formDataPostFileUploadTest({
    fileNameSource: "ASCII",
    fileBaseName: "file-for-upload-in-form-FF-[\f].txt",
  });

  formDataPostFileUploadTest({
    fileNameSource: "ASCII",
    fileBaseName: "file-for-upload-in-form-DEL-[\x7F].txt",
  });

  // The rest should be passed through unmodified:

  formDataPostFileUploadTest({
    fileNameSource: "ASCII",
    fileBaseName: "file-for-upload-in-form-ESC-[\x1B].txt",
  });

  formDataPostFileUploadTest({
    fileNameSource: "ASCII",
    fileBaseName: "file-for-upload-in-form-SPACE-[ ].txt",
  });
