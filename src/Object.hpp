#ifndef OBJECT_HPP
#define OBJECT_HPP

#include "Common.hpp"

namespace octarine {

  class InstanceInfo;

  class Object {
  private:
    const InstanceInfo* mInstanceInfo;
    const Address mInstance;

  public:
    explicit Object(InstanceInfo* vTable, Address address);
    ~Object();

    const InstanceInfo* getInstanceInfo() const;
    const Address getAddress() const;
  };

}

#endif
