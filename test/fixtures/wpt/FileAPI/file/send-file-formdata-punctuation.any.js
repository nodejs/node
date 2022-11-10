// META: title=FormData: FormData: Upload files named using punctuation
// META: script=../support/send-file-formdata-helper.js
  "use strict";

  // These have characters that undergo processing in name=,
  // filename=, and/or value; formDataPostFileUploadTest postprocesses
  // expectedEncodedBaseName for these internally.

  formDataPostFileUploadTest({
    fileNameSource: "ASCII",
    fileBaseName: "file-for-upload-in-form-QUOTATION-MARK-[\x22].txt",
  });

  formDataPostFileUploadTest({
    fileNameSource: "ASCII",
    fileBaseName: '"file-for-upload-in-form-double-quoted.txt"',
  });

  formDataPostFileUploadTest({
    fileNameSource: "ASCII",
    fileBaseName: "file-for-upload-in-form-REVERSE-SOLIDUS-[\\].txt",
  });

  // The rest should be passed through unmodified:

  formDataPostFileUploadTest({
    fileNameSource: "ASCII",
    fileBaseName: "file-for-upload-in-form-EXCLAMATION-MARK-[!].txt",
  });

  formDataPostFileUploadTest({
    fileNameSource: "ASCII",
    fileBaseName: "file-for-upload-in-form-DOLLAR-SIGN-[$].txt",
  });

  formDataPostFileUploadTest({
    fileNameSource: "ASCII",
    fileBaseName: "file-for-upload-in-form-PERCENT-SIGN-[%].txt",
  });

  formDataPostFileUploadTest({
    fileNameSource: "ASCII",
    fileBaseName: "file-for-upload-in-form-AMPERSAND-[&].txt",
  });

  formDataPostFileUploadTest({
    fileNameSource: "ASCII",
    fileBaseName: "file-for-upload-in-form-APOSTROPHE-['].txt",
  });

  formDataPostFileUploadTest({
    fileNameSource: "ASCII",
    fileBaseName: "file-for-upload-in-form-LEFT-PARENTHESIS-[(].txt",
  });

  formDataPostFileUploadTest({
    fileNameSource: "ASCII",
    fileBaseName: "file-for-upload-in-form-RIGHT-PARENTHESIS-[)].txt",
  });

  formDataPostFileUploadTest({
    fileNameSource: "ASCII",
    fileBaseName: "file-for-upload-in-form-ASTERISK-[*].txt",
  });

  formDataPostFileUploadTest({
    fileNameSource: "ASCII",
    fileBaseName: "file-for-upload-in-form-PLUS-SIGN-[+].txt",
  });

  formDataPostFileUploadTest({
    fileNameSource: "ASCII",
    fileBaseName: "file-for-upload-in-form-COMMA-[,].txt",
  });

  formDataPostFileUploadTest({
    fileNameSource: "ASCII",
    fileBaseName: "file-for-upload-in-form-FULL-STOP-[.].txt",
  });

  formDataPostFileUploadTest({
    fileNameSource: "ASCII",
    fileBaseName: "file-for-upload-in-form-SOLIDUS-[/].txt",
  });

  formDataPostFileUploadTest({
    fileNameSource: "ASCII",
    fileBaseName: "file-for-upload-in-form-COLON-[:].txt",
  });

  formDataPostFileUploadTest({
    fileNameSource: "ASCII",
    fileBaseName: "file-for-upload-in-form-SEMICOLON-[;].txt",
  });

  formDataPostFileUploadTest({
    fileNameSource: "ASCII",
    fileBaseName: "file-for-upload-in-form-EQUALS-SIGN-[=].txt",
  });

  formDataPostFileUploadTest({
    fileNameSource: "ASCII",
    fileBaseName: "file-for-upload-in-form-QUESTION-MARK-[?].txt",
  });

  formDataPostFileUploadTest({
    fileNameSource: "ASCII",
    fileBaseName: "file-for-upload-in-form-CIRCUMFLEX-ACCENT-[^].txt",
  });

  formDataPostFileUploadTest({
    fileNameSource: "ASCII",
    fileBaseName: "file-for-upload-in-form-LEFT-SQUARE-BRACKET-[[].txt",
  });

  formDataPostFileUploadTest({
    fileNameSource: "ASCII",
    fileBaseName: "file-for-upload-in-form-RIGHT-SQUARE-BRACKET-[]].txt",
  });

  formDataPostFileUploadTest({
    fileNameSource: "ASCII",
    fileBaseName: "file-for-upload-in-form-LEFT-CURLY-BRACKET-[{].txt",
  });

  formDataPostFileUploadTest({
    fileNameSource: "ASCII",
    fileBaseName: "file-for-upload-in-form-VERTICAL-LINE-[|].txt",
  });

  formDataPostFileUploadTest({
    fileNameSource: "ASCII",
    fileBaseName: "file-for-upload-in-form-RIGHT-CURLY-BRACKET-[}].txt",
  });

  formDataPostFileUploadTest({
    fileNameSource: "ASCII",
    fileBaseName: "file-for-upload-in-form-TILDE-[~].txt",
  });

  formDataPostFileUploadTest({
    fileNameSource: "ASCII",
    fileBaseName: "'file-for-upload-in-form-single-quoted.txt'",
  });
