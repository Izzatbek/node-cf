#include "cf.h"

#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <stdlib.h> // abort
#include <assert.h> // assert
#include <iostream>

namespace cf {

using namespace v8;

Nan::Persistent<Function> Loop::constructor;

Loop::Loop(CFRunLoopRef loop, CFStringRef mode) : cf_lp_(loop),
                                                  cf_mode_(mode),
                                                  uv_(uv_default_loop()),
                                                  closed_(false) {
  int r;

  // Allocate source context
  CFRunLoopSourceContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.info = this;
  ctx.perform = Perform;

  // Create source and add it to the loop
  cb_ = CFRunLoopSourceCreate(NULL, 0, &ctx);
  CFRunLoopAddSource(cf_lp_, cb_, cf_mode_);

  uv_sem_init(&sem_, 0);

  // And run our own watcher
  r = uv_thread_create(&thread_, Worker, this);
  assert(r == 0);
}


Loop::~Loop() {
  Close();
}


void Loop::Close() {
  if (closed_) return;
  closed_ = true;

  // Wait for thread to close
  uv_thread_join(&thread_);

  CFRunLoopRemoveSource(cf_lp_, cb_, cf_mode_);
  uv_sem_destroy(&sem_);

  cb_ = NULL;
}


void Loop::Worker(void* arg) {
  Loop* loop = reinterpret_cast<Loop*>(arg);

  // Wait for events on port or signal
  while (true) {
    if (loop->closed_) break;

    // Wait 1 sec maximum
    int timeout = uv_backend_timeout(loop->uv());

    struct timespec ts;
    ts.tv_sec = timeout / 1000;
    ts.tv_nsec = (timeout - (ts.tv_sec * 1000)) * 1000000;
    kevent(uv_backend_fd(loop->uv()), NULL, 0, NULL, 0, &ts);

    CFRunLoopSourceSignal(loop->cb_);
    CFRunLoopWakeUp(loop->cf_lp_);

    uv_sem_wait(&loop->sem_);
  }
}


void Loop::Perform(void* arg) {
  Loop* loop = reinterpret_cast<Loop*>(arg);

  uv_run(uv_default_loop(), UV_RUN_NOWAIT);
  uv_sem_post(&loop->sem_);
}


NAN_METHOD(Loop::New) {
  if (info.IsConstructCall()) {
    Loop* loop = new Loop(CFRunLoopGetMain(), kCFRunLoopDefaultMode);
    loop->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  } 
  else 
  {
    Local<v8::Function> cons = Nan::New(constructor);
    info.GetReturnValue().Set(cons->NewInstance(0, NULL));
  }
}


NAN_METHOD(Loop::AddRef) {
  Loop* self = ObjectWrap::Unwrap<Loop>(info.Holder());

  self->Ref();

  info.GetReturnValue().SetUndefined();
}


NAN_METHOD(Loop::RemRef) {
  Loop* self = ObjectWrap::Unwrap<Loop>(info.Holder());
  self->Unref();
  if (self->refs_ == 0) {
    self->Close();
  }

  info.GetReturnValue().SetUndefined();
}

NAN_MODULE_INIT(Loop::Init)
{
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("Loop").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(2);

  Nan::SetPrototypeMethod(tpl, "ref", AddRef);
  Nan::SetPrototypeMethod(tpl, "unref", RemRef);

  constructor.Reset(Nan::GetFunction(tpl).ToLocalChecked());
  Nan::Set(target, Nan::New("Loop").ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
}

} // namespace cf