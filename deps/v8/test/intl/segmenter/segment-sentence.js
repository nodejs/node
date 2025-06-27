// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const seg = new Intl.Segmenter([], {granularity: "sentence"})
for (const text of [
    "Hello world!", // English
    " Hello world! ",  // English with space before/after
    " Hello world? Foo bar!", // English
    "Jedovatou mambu objevila žena v zahrádkářské kolonii.", // Czech
    "Việt Nam: Nhất thể hóa sẽ khác Trung Quốc?",  // Vietnamese
    "Σοβαρές ενστάσεις Κομισιόν για τον προϋπολογισμό της Ιταλίας", // Greek
    "Решение Индии о покупке российских С-400 расценили как вызов США",  // Russian
    "הרופא שהציל נשים והנערה ששועבדה ע",  // Hebrew,
    "ترامب للملك سلمان: أنا جاد للغاية.. عليك دفع المزيد", // Arabic
    "भारत की एस 400 मिसाइल के मुकाबले पाक की थाड, जानें कौन कितना ताकतवर",  //  Hindi
    "ரெட் அலர்ட் எச்சரிக்கை; புதுச்சேரியில் நாளை அரசு விடுமுறை!", // Tamil
    "'ఉత్తర్వులు అందే వరకు ఓటర్ల తుది జాబితాను వెబ్‌సైట్లో పెట్టవద్దు'", // Telugu
    "台北》抹黑柯P失敗？朱學恒酸：姚文智氣pupu嗆大老闆", // Chinese
    "วัดไทรตีระฆังเบาลงช่วงเข้าพรรษา เจ้าอาวาสเผยคนร้องเรียนรับผลกรรมแล้ว",  // Thai
    "九州北部の一部が暴風域に入りました(日直予報士 2018年10月06日) - 日本気象協会 tenki.jp",  // Japanese
    "법원 “다스 지분 처분권·수익권 모두 MB가 보유”", // Korean
    ]) {
  const segments = seg.segment(text);
  let results = [];
  var pos = -1;
  for (let s of segments) {
    assertEquals(["segment", "index", "input"], Object.keys(s));
    assertEquals(typeof s.index, "number");
    assertEquals(typeof s.segment, "string");
    assertEquals(typeof s.input, "string");
    assertEquals(text, s.input);
    assertEquals(text.substring(s.index, s.index + s.segment.length),
        s.segment);
    assertTrue(pos < s.index);
    pos = s.index;
    results.push(s.segment);
  }
  assertTrue(pos < text.length);
  assertEquals(text, results.join(""));
}
