#include "builtin.hpp"
#include "global_cache.hpp"

namespace rubinius {
  void SendSite::init(STATE) {
    GO(send_site).set(state->new_class("SendSite", 0));
    G(send_site)->set_object_type(SendSiteType);
  }

  SendSite* SendSite::create(STATE, OBJECT name) {
    SendSite* ss = (SendSite*)state->new_struct(G(send_site), sizeof(SendSite));
    SET(ss, name, name);
    SET(ss, sender, Qnil);
    SET(ss, selector, Selector::lookup(state, name));
    ss->hits = ss->misses = 0;

    ss->initialize(state);
    ss->selector->associate(state, ss);

    return ss;
  }

  void SendSite::initialize(STATE) {
    if(resolver) delete resolver;
    specialized = false;
    resolver = new GlobalCacheResolver;
  }

  /* Indicates that the object hasn't been specialized in anyway */
  bool SendSite::basic_p(STATE) {
    return !specialized;
  }

  /* Use the information within +this+ to populate +msg+. Returns
   * true if +msg+ was populated. */

  bool SendSite::locate(STATE, Message& msg) {
    if(!resolver->resolve(state, msg)) {
      msg.name = G(sym_method_missing);
      if(!resolver->resolve(state, msg)) {
        return false;
      }
    }

    return true;
  }

  /* Fill in details about +msg+ by looking up the class heirarchy
   * and in method tables. Returns true if lookup was successful
   * and +msg+ is now filled in. */

  bool HierarchyResolver::resolve(STATE, Message& msg) {
    Module* module = msg.lookup_from;
    OBJECT entry;
    MethodVisibility* vis;

    do {
      entry = module->method_table->fetch(state, msg.name);

      /* Nothing, there? Ok, keep looking. */
      if(entry->nil_p()) goto keep_looking;

      /* A 'false' method means to terminate method lookup.
       * (eg. undef_method) */
      if(entry == Qfalse) return false;

      vis = try_as<MethodVisibility>(entry);

      /* If this was a private send, then we can handle use
       * any method seen. */
      if(msg.priv) {
        /* nil means that the actual method object is 'up' from here */
        if(vis && vis->method->nil_p()) goto keep_looking;

        msg.method = entry;
        msg.module = module;
        break;
      } else if(vis) {
        /* The method is private, but this wasn't a private send. */
        if(vis->private_p(state)) {
          return false;
        } else if(vis->protected_p(state)) {
          /* The method is protected, but it's not being called from
           * the same module */
          if(!msg.current_self->kind_of_p(state, module)) {
            return false;
          }
        }

        /* The method was callable, but we need to keep looking
         * for the implementation, so make the invocation bypass all further
         * visibility checks */
        if(vis->method->nil_p()) {
          msg.priv = true;
          goto keep_looking;
        }

        msg.method = entry;
        msg.module = module;
        break;
      } else {
        msg.method = entry;
        msg.module = module;
        break;
      }

keep_looking:
      module = module->superclass;

      /* No more places to look, we couldn't find it. */
      if(module->nil_p()) return false;
    } while(1);

    return true;
  }

  bool GlobalCacheResolver::resolve(STATE, Message& msg) {
    struct GlobalCache::cache_entry* entry;

    entry = state->global_cache->lookup(msg.lookup_from, msg.name);
    if(entry) {
      if(msg.priv || entry->is_public) {
        msg.method = entry->method;
        msg.module = entry->module;
        return true;
      }
    }

    if(HierarchyResolver::resolve(state, msg)) {
      state->global_cache->retain(state, msg.lookup_from, msg.name,
          msg.module, msg.method);
      return true;
    }

    return false;
  }

  bool SpecializedResolver::resolve(STATE, Message& msg) {
    if(msg.lookup_from == klass) {
      msg.module = mod;
      msg.method = method;
      return true;
    }

    if(GlobalCacheResolver::resolve(state, msg)) {
      mod = msg.module;
      method = msg.method;
      klass = msg.lookup_from;
      return true;
    }

    return false;
  }
};