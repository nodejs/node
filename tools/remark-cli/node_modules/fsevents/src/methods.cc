/*
** © 2014 by Philipp Dunkel <pip@pipobscure.com>
** Licensed under MIT License.
*/

void FSEvents::emitEvent(const char *path, UInt32 flags, UInt64 id) {
  if (!handler) return;
  Nan::HandleScope handle_scope;
  v8::Local<v8::Value> argv[] = {
    Nan::New<v8::String>(path).ToLocalChecked(),
    Nan::New<v8::Number>(flags),
    Nan::New<v8::Number>(id)
  };
  handler->Call(3, argv);
}

NAN_METHOD(FSEvents::New) {
  Nan::Utf8String *path = new Nan::Utf8String(info[0]);
  Nan::Callback *callback = new Nan::Callback(info[1].As<v8::Function>());

  FSEvents *fse = new FSEvents(**path, callback);
  fse->Wrap(info.This());

  info.GetReturnValue().Set(info.This());
}

NAN_METHOD(FSEvents::Stop) {
  FSEvents* fse = node::ObjectWrap::Unwrap<FSEvents>(info.This());

  fse->threadStop();
  fse->asyncStop();

  info.GetReturnValue().Set(info.This());
}

NAN_METHOD(FSEvents::Start) {
  FSEvents* fse = node::ObjectWrap::Unwrap<FSEvents>(info.This());
  fse->asyncStart();
  fse->threadStart();

  info.GetReturnValue().Set(info.This());
}
