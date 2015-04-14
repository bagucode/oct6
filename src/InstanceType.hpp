#ifndef INSTANCETYPE_HPP
#define INSTANCETYPE_HPP

namespace octarine {

  class Type;
  class StorageManager;

  class InstanceType {
  private:
    const Type* mType;
    const StorageManager* mStorageManager;

  public:
    explicit InstanceType(Type* type, StorageManager* storageManager);
    ~InstanceType();

    const Type* getType() const;
    const StorageManager* getStorageManager() const;
  };

}

#endif
