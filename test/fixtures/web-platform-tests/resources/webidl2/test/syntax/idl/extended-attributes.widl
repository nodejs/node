// Extracted from http://www.w3.org/TR/2015/WD-service-workers-20150205/

[Global=(Worker,ServiceWorker), Exposed=ServiceWorker]
interface ServiceWorkerGlobalScope : WorkerGlobalScope {

};

// Conformance with ExtendedAttributeList grammar in http://www.w3.org/TR/WebIDL/#idl-extended-attributes
// Section 3.11
[IntAttr=0, FloatAttr=3.14, StringAttr="abc"]
interface IdInterface {};

// Extracted from http://www.w3.org/TR/2016/REC-WebIDL-1-20161215/#Constructor on 2017-5-18 with whitespace differences
[
  Constructor,
  Constructor(double radius)
]
interface Circle {
  attribute double r;
  attribute double cx;
  attribute double cy;
  readonly attribute double circumference;
};

// Extracted from https://heycam.github.io/webidl/#idl-annotated-types on 2017-12-15
[Exposed=Window]
interface I {
    attribute [XAttr] (long or Node) attrib;
};
