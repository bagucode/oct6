#ifndef OBJECT_HPP
#define OBJECT_HPP

#include "Common.hpp"

namespace octarine {

  class VTable;

  // "Fat pointer" for runtime polymorphism
  class Object {
    struct sVTable* vtable;
    Address object;
  };

}

#endif
