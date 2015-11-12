#ifndef _SRC_CF_H_
#define _SRC_CF_H_

#include <uv.h>
#include <CoreFoundation/CoreFoundation.h>
#include <nan.h>

namespace cf {

class Loop : public Nan::ObjectWrap {

public:
  static NAN_MODULE_INIT(Init);

 protected:

  static Nan::Persistent<v8::Function> constructor;

  Loop(CFRunLoopRef loop, CFStringRef mode);
  ~Loop();
  void Close();

  inline uv_loop_t* uv() const { return uv_; }

  static NAN_METHOD(New);
  static NAN_METHOD(AddRef);
  static NAN_METHOD(RemRef);

  static void Worker(void* arg);
  static void Perform(void* arg);

  CFRunLoopRef cf_lp_;
  CFStringRef cf_mode_;
  CFRunLoopSourceRef cb_;

  uv_loop_t* uv_;
  uv_sem_t sem_;
  uv_thread_t thread_;

  bool closed_;
};

} // namespace cf

#endif // _SRC_CF_H_