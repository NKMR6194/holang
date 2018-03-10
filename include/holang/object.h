#pragma once

#include "holang/code.h"
#include <iostream>
#include <map>
#include <string>
#include <vector>

class Klass;
struct Func;
struct Value;

class Object {
public:
  Klass *klass;
  std::map<std::string, Func *> methods;
  std::map<std::string, Object *> fields;

public:
  Func *find_method(const std::string &method_name);
  void set_method(const std::string &name, Func *func) {
    methods.emplace(name, func);
  }
  void set_field(const std::string &name, Object *obj) {
    fields.emplace(name, obj);
  }
  virtual const std::string to_s() { return "<Object>"; }
};

class Klass : public Object {
  std::string name;

public:
  Klass(std::string name) : name(name) {}
  Klass(const char name[]) : name(name) {}
  static Klass Int;
  static Klass String;
};

enum FuncType {
  FBUILTIN,
  FUSERDEF,
};

using NativeFunc = std::function<Value(Value *, Value *, int)>;

struct Func {
  FuncType type;
  NativeFunc native;
  std::vector<Code> body;

  // Func() {}
  Func(const Func &func)
      : type(func.type), native(func.native), body(func.body) {}
  Func(NativeFunc native) : type(FBUILTIN), native(native) {}
  Func(const std::vector<Code> &body) : type(FUSERDEF), body(body) {}
};
