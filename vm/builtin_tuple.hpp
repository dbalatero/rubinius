#ifndef RBX_BUILTIN_TUPLE_HPP
#define RBX_BUILTIN_TUPLE_HPP

namespace rubinius {
  class Tuple : public BuiltinType {
  public:
    const static size_t fields = 0;
    const static object_type type = TupleType;

    static Tuple* create(STATE, size_t fields);
    static Tuple* from(STATE, size_t fields, ...);

    OBJECT at(size_t index) {
      if(field_count <= index) {
        throw new ObjectBoundsExceeded(this, index);
      }
      return field[index];
    }

    OBJECT put(STATE, size_t idx, OBJECT val);
    void copy_from(STATE, Tuple* other, int start, int end);

    class Info : public TypeInfo {
    public:
      Info(object_type type) : TypeInfo(type) { }
    };
  };
};

#endif