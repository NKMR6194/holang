#include "./object.hh"

Func *Object::find_method(const std::string &method_name) {
  auto it = methods.find(method_name);
  if (it != methods.end()) {
    return it->second;
  } else if (klass != nullptr) {
    return klass->find_method(method_name);
  } else {
    std::cerr << "method unmatch" << std::endl;
    exit(1);
  }
}

Klass Klass::Int{"Int"};

Func *Value::find_method(const std::string &name) {
  switch (type) {
  case VOBJECT:
    return objval->find_method(name);
  case VINT:
    return Klass::Int.find_method(name);
  default:
    std::cerr << "find_method: " << this->to_s() << std::endl;
    exit(1);
  }
}
