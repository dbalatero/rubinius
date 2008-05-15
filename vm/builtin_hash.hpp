#ifndef RBX_HASH_HPP
#define RBX_HASH_HPP

namespace rubinius {

  class LookupTable;

  #define HASH_MINSIZE 16
  class Hash : public BuiltinType {
    public:
    const static size_t fields = 7;
    const static object_type type = HashType;

    OBJECT __ivars__; // slot
    OBJECT keys; // slot
    Tuple* values; // slot
    FIXNUM bins; // slot
    FIXNUM entries; // slot
    OBJECT default_value; // slot
    OBJECT default_proc; // slot

    static Hash* create(STATE, size_t size = HASH_MINSIZE);
    void   setup(STATE, size_t size);
    Hash*  dup(STATE);
    static Tuple* entry_new(STATE, hashval hsh, OBJECT key, OBJECT data);
    static Tuple* entry_append(STATE, Tuple* top, Tuple* nxt);
    OBJECT add_entry(STATE, hashval hsh, Tuple* ent);
    void   redistribute(STATE);
    Tuple* find_entry(STATE, hashval hsh);
    OBJECT add(STATE, hashval hsh, OBJECT key, OBJECT data);
    OBJECT set(STATE, OBJECT key, OBJECT val);
    OBJECT get(STATE, hashval hsh);
    int    lookup(STATE, OBJECT key, hashval hash, OBJECT *value);
    int    lookup2(STATE, int (*compare)(STATE, OBJECT, OBJECT),
        OBJECT key, hashval hash, OBJECT *value);
    void   assign(STATE, int (*compare)(STATE, OBJECT, OBJECT),
        OBJECT key, hashval hash, OBJECT value);
    OBJECT get_undef(STATE, hashval hsh);
    OBJECT remove(STATE, hashval hsh);

    static Hash*  from_tuple(STATE, Tuple* tup);
    static Tuple* csm_new(STATE);
    static OBJECT csm_find(STATE, Tuple* csm, OBJECT key);
    static OBJECT csm_add(STATE, Tuple* csm, OBJECT key, OBJECT val);
    static Hash*  csm_into_hash(STATE, Tuple* csm);
    static LookupTable* csm_into_lookuptable(STATE, Tuple* csm);

    class Info : public TypeInfo {
    public:
      Info(object_type type) : TypeInfo(type) { }
      virtual void set_field(STATE, OBJECT target, size_t index, OBJECT val);
      virtual OBJECT get_field(STATE, OBJECT target, size_t index);
    };

  };

#define HASH_MAX_DENSITY 0.75
#define hash_redistribute_p(hash) (hash->entries->n2i() >= HASH_MAX_DENSITY * hash->bins->n2i())
};

#endif