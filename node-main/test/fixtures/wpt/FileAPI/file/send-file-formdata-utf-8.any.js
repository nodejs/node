// META: title=FormData: FormData: Upload files in UTF-8 fetch()
// META: script=../support/send-file-formdata-helper.js
  "use strict";

  formDataPostFileUploadTest({
    fileNameSource: "ASCII",
    fileBaseName: "file-for-upload-in-form.txt",
  });

  formDataPostFileUploadTest({
    fileNameSource: "x-user-defined",
    fileBaseName: "file-for-upload-in-form-\uF7F0\uF793\uF783\uF7A0.txt",
  });

  formDataPostFileUploadTest({
    fileNameSource: "windows-1252",
    fileBaseName: "file-for-upload-in-form-â˜ºðŸ˜‚.txt",
  });

  formDataPostFileUploadTest({
    fileNameSource: "JIS X 0201 and JIS X 0208",
    fileBaseName: "file-for-upload-in-form-★星★.txt",
  });

  formDataPostFileUploadTest({
    fileNameSource: "Unicode",
    fileBaseName: "file-for-upload-in-form-☺😂.txt",
  });

  formDataPostFileUploadTest({
    fileNameSource: "Unicode",
    fileBaseName: `file-for-upload-in-form-${kTestChars}.txt`,
  });
