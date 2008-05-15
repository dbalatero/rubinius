#include "objects.hpp"

namespace rubinius {
  VMMethod::VMMethod(CompiledMethod* meth) : original(meth) {
    total = meth->iseq->opcodes->field_count;

    opcodes = new opcode[total];

    for(size_t index = 0; index < total; index++) {
      OBJECT val = meth->iseq->opcodes->at(index);
      if(val->nil_p()) {
        opcodes[index] = 0;
      } else {
        opcodes[index] = as<Fixnum>(val)->n2i();
      }
    }
  }

  VMMethod::VMMethod(size_t fields) {
    opcodes = new opcode[fields];
  }

  VMMethod::~VMMethod() {
    delete[] opcodes;
  }

  void VMMethod::specialize(TypeInfo* ti) {
    for(size_t i = 0; i < total; i++) {
      opcode op = opcodes[i];
      
      if(op == InstructionSequence::insn_push_ivar) {
        native_int idx = opcodes[i + 1];
        native_int sym = as<Symbol>(original->literals->at(idx))->index();

        TypeInfo::Slots::iterator it = ti->slots.find(sym);
        if(it != ti->slots.end()) {
          opcodes[i] = InstructionSequence::insn_push_my_field;
          opcodes[++i] = it->second;
        }
      }
    }
  }
}