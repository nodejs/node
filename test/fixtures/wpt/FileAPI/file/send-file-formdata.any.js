// META: title=FormData: Upload ASCII-named file in UTF-8 form
// META: script=../support/send-file-formdata-helper.js
  "use strict";

  formDataPostFileUploadTest({
    fileNameSource: "ASCII",
    fileBaseName: "file-for-upload-in-form.txt",
  });
