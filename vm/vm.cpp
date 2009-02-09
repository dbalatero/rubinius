#include "vm.hpp"
#include "objectmemory.hpp"
#include "event.hpp"
#include "global_cache.hpp"
#include "llvm.hpp"

#include "vm/object_utils.hpp"

#include "builtin/class.hpp"
#include "builtin/contexts.hpp"
#include "builtin/fixnum.hpp"
#include "builtin/list.hpp"
#include "builtin/symbol.hpp"
#include "builtin/thread.hpp"
#include "builtin/tuple.hpp"
#include "builtin/string.hpp"
#include "builtin/taskprobe.hpp"

#include "config_parser.hpp"
#include "config.h"
#include "timing.hpp"
#include "vm_manager.hpp"

#include "native_thread.hpp"

#include <iostream>
#include <signal.h>

// Reset macros since we're inside state
#undef G
#undef GO
#define G(whatever) globals.whatever.get()
#define GO(whatever) globals.whatever

namespace rubinius {

  SharedState::~SharedState() {
    if(!initialized_) return;

    delete om;
    delete global_cache;
    delete user_config;

#ifdef ENABLE_LLVM
    if(!reuse_llvm) llvm_cleanup();
#endif
  }

  VM* SharedState::new_vm() {
    VM* vm = manager_.create_vm(this);
    vms_[vm->id()] = vm;
    return vm;
  }

  void SharedState::remove_vm(VM* vm) {
    VMMap::iterator i = vms_.find(vm->id());
    if(i != vms_.end()) {
      vms_.erase(i);
    }
  }

  void SharedState::add_call_frames(CallFrameList& call_frames, VM* current) {
    for(VMMap::const_iterator i = vms_.begin();
        i != vms_.end();
        i++) {
      VM* vm = i->second;
      if(vm != current) {
        CallFrame* frame = vm->saved_call_frame();
        if(frame) call_frames.push_back(frame);
      }
    }
  }

  VM::VM(SharedState& shared, int id)
    : id_(id)
    , saved_call_frame_(0)
    , shared(shared)
    , globals(shared.globals)
    , om(shared.om)
    , global_cache(shared.global_cache)
    , config(shared.config)
    , interrupts(shared.interrupts)
    , symbols(shared.symbols)
    , user_config(shared.user_config)
    , thread(this, (Thread*)Qnil)
    , current_mark(NULL)
    , reuse_llvm(true)
    , use_safe_position(false)
  {}

  void VM::initialize(size_t bytes)
  {
    config.compile_up_front = false;
    config.jit_enabled = false;
    config.dynamic_interpreter_enabled = false;

    VM::register_state(this);

    user_config = new ConfigParser();
    shared.user_config = user_config;

    om = new ObjectMemory(this, bytes);
    shared.om = om;

    probe.set(Qnil, &globals.roots);

    global_cache = new GlobalCache;
    shared.global_cache = global_cache;

    this->boot();

    shared.set_initialized();
  }

  void VM::boot() {
#ifdef USE_USAGE_JIT
    if(user_config->find("rbx.jit")) {
      config.jit_enabled = true;
    }
#endif

#ifdef USE_DYNAMIC_INTERPRETER
    if(user_config->find("rbx.dyni")) {
      config.dynamic_interpreter_enabled = true;
    }
#endif

    MethodContext::initialize_cache(this);
    TypeInfo::auto_learn_fields(this);

    bootstrap_ontology();

    /* @todo Using a single default loop, revisit when many loops.
     * @todo This needs to be handled through the environment.
     * (disabled epoll backend as it frequently caused hangs on epoll_wait)
     */
    signal_events = new event::Loop(EVFLAG_FORKCHECK | EVBACKEND_SELECT | EVBACKEND_POLL);
    events = signal_events;

    signal_events->start(new event::Child::Event(this));

    VMMethod::init(this);

#ifdef ENABLE_LLVM
    VMLLVMMethod::init("vm/instructions.bc");
#endif
    boot_threads();

    // Force these back to false because creating the default Thread
    // sets them to true.
    interrupts.use_preempt = false;
    interrupts.enable_preempt = false;
  }

  // HACK so not thread safe or anything!
  static VM* __state = NULL;

  VM* VM::current_state() {
    return __state;
  }

  void VM::register_state(VM *vm) {
    __state = vm;
  }

  void VM::boot_threads() {
    thread.set(Thread::create(this, this), &globals.roots);

    thread->sleep(this, Qfalse);
    globals.current_task.set(thread->task());
  }

  Object* VM::new_object_typed(Class* cls, size_t bytes, object_type type) {
    return om->new_object_typed(cls, bytes, type);
  }

  Object* VM::new_object_from_type(Class* cls, TypeInfo* ti) {
    return om->new_object_typed(cls, ti->instance_size, ti->type);
  }

  Class* VM::new_basic_class(Class* sup) {
    Class *cls = new_object<Class>(G(klass));
    if(sup->nil_p()) {
      cls->instance_type(this, Fixnum::from(ObjectType));
    } else {
      cls->instance_type(this, sup->instance_type()); // HACK test that this is always true
    }
    cls->superclass(this, sup);

    return cls;
  }

  Class* VM::new_class(const char* name) {
    return new_class(name, G(object), G(object));
  }

  Class* VM::new_class(const char* name, Class* super_class) {
    return new_class(name, super_class, G(object));
  }

  Class* VM::new_class(const char* name, Class* sup, Module* under) {
    Class* cls = new_basic_class(sup);
    cls->setup(this, name, under);

    // HACK test that we've got the MOP setup properly
    MetaClass::attach(this, cls, sup->metaclass(this));
    return cls;
  }

  Class* VM::new_class_under(const char* name, Module* under) {
    return new_class(name, G(object), under);
  }

  Module* VM::new_module(const char* name, Module* under) {
    Module *mod = new_object<Module>(G(module));
    mod->setup(this, name, under);
    return mod;
  }


  Symbol* VM::symbol(const char* str) {
    return symbols.lookup(this, str);
  }

  Symbol* VM::symbol(String* str) {
    return symbols.lookup(this, str);
  }

  Symbol* VM::symbol(std::string str) {
    return symbols.lookup(this, str);
  }

  void type_assert(STATE, Object* obj, object_type type, const char* reason) {
    if((obj->reference_p() && obj->obj_type != type)
        || (type == FixnumType && !obj->fixnum_p())) {
      Exception::type_error(state, type, obj, reason);
    }
  }

  TypeInfo* VM::find_type(int type) {
    return om->type_info[type];
  }

  Thread *VM::current_thread() {
    return globals.current_thread.get();
  }

  void VM::run_gc_soon() {
    om->collect_young_now = true;
    om->collect_mature_now = true;
    interrupts.check = true;
  }

  void VM::collect(CallFrame* call_frame) {
    uint64_t start = get_current_time();

    CallFrameList frames;
    frames.push_back(call_frame);
    shared.add_call_frames(frames, this);

    om->collect_young(globals.roots, frames);
    om->collect_mature(globals.roots, frames);

    stats.time_in_gc += (get_current_time() - start);
  }

  void VM::collect_maybe(CallFrame* call_frame) {
    CallFrameList frames;
    frames.push_back(call_frame);
    shared.add_call_frames(frames, this);

    if(om->collect_young_now) {
      om->collect_young_now = false;

      uint64_t start = get_current_time();
      om->collect_young(globals.roots, frames);
      stats.time_in_gc += (get_current_time() - start);

      global_cache->clear();
    }

    if(om->collect_mature_now) {
      om->collect_mature_now = false;

      uint64_t start = get_current_time();
      om->collect_mature(globals.roots, frames);
      stats.time_in_gc += (get_current_time() - start);

      global_cache->clear();
    }
  }

  void VM::set_const(const char* name, Object* val) {
    globals.object->set_const(this, (char*)name, val);
  }

  void VM::set_const(Module* mod, const char* name, Object* val) {
    mod->set_const(this, (char*)name, val);
  }

  void VM::print_backtrace() {
    abort();
  }

  void VM::raise_exception_safely(Exception* exc) {
    safe_position_data.exc = exc;
    siglongjmp(safe_position, cReasonException);
    // Never reached.
  }

  void VM::raise_typeerror_safely(TypeError* err) {
    safe_position_data.type_error = err;
    siglongjmp(safe_position, cReasonTypeError);
    // Never reached.
  }

  void VM::raise_assertion_safely(Assertion* err) {
    safe_position_data.assertion = err;
    siglongjmp(safe_position, cReasonAssertion);
    // Never reached.
  }

  // Trampoline to call scheduler_loop()
  static void* __thread_tramp__(void* arg) {
    VM* state = static_cast<VM*>(arg);
    state->scheduler_loop();
    return NULL;
  }

  // Create the preemption thread and call scheduler_loop() in the new thread
  void VM::setup_preemption() {
    if(pthread_create(&preemption_thread, NULL, __thread_tramp__, this) != 0) {
      std::cout << "Unable to create preemption thread!\n";
    }
  }

  // Runs forever, telling the VM to reschedule threads ever 10 milliseconds
  void VM::scheduler_loop() {
    // First off, we don't want this thread ever receiving a signal.
    sigset_t mask;
    sigfillset(&mask);
    if(pthread_sigmask(SIG_BLOCK, &mask, NULL) != 0) {
      abort();
    }

    struct timespec requested;
    struct timespec actual;

    requested.tv_sec = 0;
    requested.tv_nsec = 10000000; // 10 milliseconds

    for(;;) {
      nanosleep(&requested, &actual);
      if(interrupts.enable_preempt) {
        interrupts.reschedule = true;
        interrupts.check_events = true;
      }
    }
  }

  void VM::install_waiter(Waiter& waiter) {
    waiter_ = &waiter;
  }

  bool VM::wakeup() {
    if(waiter_) {
      waiter_->wakeup();
      return true;
    }

    return false;
  }

  /* For debugging. */
  extern "C" {
    void __printbt__() {
      VM::current_state()->print_backtrace();
    }
  }
};
