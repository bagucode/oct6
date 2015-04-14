#ifndef INSTANCEINFO_HPP
#define INSTANCEINFO_HPP

namespace octarine {

  class InstanceType;
  class FunctionArray;

  class InstanceInfo {
  private:
    const InstanceType* mInstanceType;
    const FunctionArray* mFunctions;

  public:
    explicit InstanceInfo(InstanceType* instanceType, FunctionArray* functions);
    ~InstanceInfo();

    const InstanceType* getInstanceType() const;
    const FunctionArray* getFunctions() const;
  };

}

#endif

