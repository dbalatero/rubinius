#ifndef RBX_PARSER_LOCALSTATE_HPP
#define RBX_PARSER_LOCALSTATE_HPP

#include "parser/var_table.hpp"

namespace rubinius {
  namespace parser {
    struct LocalState {
      LocalState* prev;
      var_table variables;
      var_table block_vars;

      LocalState(LocalState* prev)
        : prev(prev)
        , variables(var_table_create())
        , block_vars(0)
      {}

      ~LocalState() {
        var_table_destroy(variables);
        if(block_vars) {
          var_table_destroy(block_vars);
        }
      }

      bool blocks_p() {
        return block_vars != NULL;
      }

      static LocalState* push(LocalState* cur) {
        return new LocalState(cur);
      }

      static LocalState* pop(LocalState* cur) {
        LocalState* tmp = cur->prev;
        delete cur;
        return tmp;
      }
    };
  }
}

#endif
