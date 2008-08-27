#ifndef RBX_BUILTIN_NATIVEFUNCTION_HPP
#define RBX_BUILTIN_NATIVEFUNCTION_HPP

#include "builtin/executable.hpp"

#include <ffi.h>

namespace rubinius {
  class NativeFunction : public Executable {
  public:
    static const size_t fields = 7;
    static const object_type type = NativeFuncType;

    OBJECT name; // slot
    SYMBOL file; // slot
    MemoryPointer* data; // slot
    FIXNUM required; // slot

    static void* find_symbol(STATE, OBJECT library, String* name);

    static size_t type_size(size_t type);

    // Ruby.primitive :nativefunction_type_size
    static FIXNUM type_size_prim(STATE, FIXNUM type);

    static void init(STATE);

    static NativeFunction* create(STATE, OBJECT name, int args);

    // Ruby.primitive :nativefunction_bind
    static NativeFunction* bind(STATE, OBJECT library, String* name, Array* args, OBJECT ret);

    void bind(STATE, int arg_count, int *arg_types, int ret_type, void* func);
    void **marshal_arguments(STATE, Message *msg);
    OBJECT call(STATE, Message* msg);

    struct ffi_stub {
      ffi_cif cif;
      size_t arg_count;
      int *arg_types;
      int ret_type;
      void *ep;
    };

    class Info : public TypeInfo {
    public:
      BASIC_TYPEINFO(TypeInfo)
    };

  };

}

#endif