// META: title=Encoding API: replacement encoding
// META: script=resources/encodings.js
// META: script=resources/decoding-helpers.js

const replacement_labels = [];
encodings_table.forEach(section => {
  section.encodings
         .filter(encoding => encoding.name === 'replacement')
         .forEach(encoding => {
           encoding.labels.forEach(label => { replacement_labels.push(label); })
         });
});

replacement_labels.forEach(label => {
  decode_test(
    label,
    '%41%42%43%61%62%63%31%32%33%A0',
    'U+FFFD',
    `${label} - non-empty input decodes to one replacement character.`);

  decode_test(
    label,
    '',
    '', `${label} - empty input decodes to empty output.`);
});
