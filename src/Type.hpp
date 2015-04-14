#ifndef TYPE_HPP
#define TYPE_HPP

#include "Common.hpp"

#include <string>

using std::string;

namespace octarine {

  class FieldArray;

  class Type {
  private:
    string mName;
    FieldArray* mFields;

  public:
    explicit Type(string name, FieldArray* fields);
    ~Type();

    string getName();
    FieldArray* getFields();
  };

}

#endif
