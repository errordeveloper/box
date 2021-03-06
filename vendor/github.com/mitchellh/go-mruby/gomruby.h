/*
 * This header exists to simplify the headers that are included within
 * the Go files. This header should include all the necessary headers
 * for the compilation of the Go library.
 * */

#ifndef _GOMRUBY_H_INCLUDED
#define _GOMRUBY_H_INCLUDED

#include <mruby.h>
#include <mruby/array.h>
#include <mruby/class.h>
#include <mruby/compile.h>
#include <mruby/error.h>
#include <mruby/irep.h>
#include <mruby/gc.h>
#include <mruby/hash.h>
#include <mruby/proc.h>
#include <mruby/string.h>
#include <mruby/throw.h>
#include <mruby/value.h>
#include <mruby/variable.h>

//-------------------------------------------------------------------
// Helpers to deal with calling back into Go.
//-------------------------------------------------------------------
// This is declard in func.go and is a way for us to call back into
// Go to execute a method.
extern mrb_value goMRBFuncCall(mrb_state*, mrb_value*, mrb_value*);

// This calls into Go with a similar signature to mrb_func_t. We have to
// change it slightly because cgo can't handle the union type of mrb_value,
// so we pass in a pointer instead. Additionally, the result is also a
// pointer to work around Go's confusion with unions.
static inline mrb_value _goMRBFuncCall(mrb_state *s, mrb_value self) {
    mrb_value exc = mrb_nil_value();
    mrb_value result = goMRBFuncCall(s, &self, &exc);
    // We raise if we got an exception. We have to raise from here and
    // not from within Go because it messes with Go's calling conventions,
    // resulting in a broken stack.
    if (!mrb_nil_p(exc)) {
        mrb_exc_raise(s, exc);
    }

    return result;
}

// This method is used as a way to get a valid mrb_func_t that actually
// just calls back into Go.
static inline mrb_func_t _go_mrb_func_t() {
    return &_goMRBFuncCall;
}

//-------------------------------------------------------------------
// Helpers to deal with calling into Ruby (C)
//-------------------------------------------------------------------
// These are some really horrible C macros that are used to wrap
// various mruby C API function calls so that we catch the exceptions.
// If we let exceptions through then the longjmp will cause a Go stack
// split.
#define GOMRUBY_EXC_PROTECT_START \
    struct mrb_jmpbuf *prev_jmp = mrb->jmp; \
    struct mrb_jmpbuf c_jmp; \
    mrb_value result = mrb_nil_value(); \
    MRB_TRY(&c_jmp) { \
        mrb->jmp = &c_jmp;

#define GOMRUBY_EXC_PROTECT_END \
        mrb->jmp = prev_jmp; \
    } MRB_CATCH(&c_jmp) { \
        mrb->jmp = prev_jmp; \
        result = mrb_nil_value();\
    } MRB_END_EXC(&c_jmp); \
    mrb_gc_protect(mrb, result); \
    return result;

static mrb_value _go_mrb_load_string(mrb_state *mrb, const char *s) {
    GOMRUBY_EXC_PROTECT_START
    result = mrb_load_string(mrb, s);
    GOMRUBY_EXC_PROTECT_END
}

static mrb_value _go_mrb_yield_argv(mrb_state *mrb, mrb_value b, mrb_int argc, const mrb_value *argv) {
    GOMRUBY_EXC_PROTECT_START
    result = mrb_yield_argv(mrb, b, argc, argv);
    GOMRUBY_EXC_PROTECT_END
}

//-------------------------------------------------------------------
// Helpers to deal with getting arguments
//-------------------------------------------------------------------
// This is declard in args.go
extern void goGetArgAppend(mrb_value*);

// This gets all arguments given to a function call and adds them to
// the accumulator in Go.
static inline int _go_mrb_get_args_all(mrb_state *s) {
    mrb_value *argv;
    mrb_value block;
    int argc, i, count;

    count = mrb_get_args(s, "*&", &argv, &argc, &block);
    for (i = 0; i < argc; i++) {
        goGetArgAppend(&argv[i]);
    }

    if (!mrb_nil_p(block)) {
        goGetArgAppend(&block);
    }

    return count;
}

//-------------------------------------------------------------------
// Misc. helpers
//-------------------------------------------------------------------

// This is used to help calculate the "send" value for the parser,
// since pointer arithmetic like this is hard in Go.
static inline const char *_go_mrb_calc_send(const char *s) {
    return s + strlen(s);
}

// Sets the capture_errors field on mrb_parser_state. Go can't access bit
// fields.
static inline void
_go_mrb_parser_set_capture_errors(struct mrb_parser_state *p, mrb_bool v) {
    p->capture_errors = v;
}

//-------------------------------------------------------------------
// Functions below here expose defines or inline functions that were
// otherwise inaccessible to Go directly.
//-------------------------------------------------------------------

static inline mrb_aspec _go_MRB_ARGS_ANY() {
    return MRB_ARGS_ANY();
}

static inline mrb_aspec _go_MRB_ARGS_ARG(int r, int o) {
    return MRB_ARGS_ARG(r, o);
}

static inline mrb_aspec _go_MRB_ARGS_BLOCK() {
    return MRB_ARGS_BLOCK();
}

static inline mrb_aspec _go_MRB_ARGS_NONE() {
    return MRB_ARGS_NONE();
}

static inline mrb_aspec _go_MRB_ARGS_OPT(int n) {
    return MRB_ARGS_OPT(n);
}

static inline mrb_aspec _go_MRB_ARGS_REQ(int n) {
    return MRB_ARGS_REQ(n);
}

static inline float _go_mrb_float(mrb_value o) {
    return mrb_float(o);
}

static inline int _go_mrb_fixnum(mrb_value o) {
    return mrb_fixnum(o);
}

static inline struct RBasic *_go_mrb_basic_ptr(mrb_value o) {
    return mrb_basic_ptr(o);
}

static inline struct RProc *_go_mrb_proc_ptr(mrb_value o) {
    return mrb_proc_ptr(o);
}

static inline enum mrb_vtype _go_mrb_type(mrb_value o) {
    return mrb_type(o);
}

static inline mrb_bool _go_mrb_nil_p(mrb_value o) {
    return mrb_nil_p(o);
}

static inline struct RClass *_go_mrb_class_ptr(mrb_value o) {
  return mrb_class_ptr(o);
}

static inline void _go_set_gc(mrb_state *m, int val) {
  mrb_gc *gc = &m->gc;
  gc->disabled = val;
}

static inline void _go_disable_gc(mrb_state *m) {
  _go_set_gc(m, 1);
}

static inline void _go_enable_gc(mrb_state *m) {
  _go_set_gc(m, 0);
}

// this function returns 1 if the value is dead, aka reaped or otherwise
// terminated by the GC.
static inline int _go_isdead(mrb_state *m, mrb_value o) {
  // immediate values such as Fixnums and symbols are never to be garbage
  // collected, so converting them to a basic pointer yields an invalid one.
  // This pattern is seen in the mruby source's gc.c.
  if mrb_immediate_p(o) {
    return 0;
  }

  struct RBasic *ptr = mrb_basic_ptr(o);

  // I don't actually know this is a potential condition but better safe than sorry.
  if (ptr == NULL) {
    return 1;
  }

  return mrb_object_dead_p(m, ptr);
}

static inline int _go_gc_live(mrb_state *m) {
  mrb_gc *gc = &m->gc;
  return gc->live;
}

#endif
