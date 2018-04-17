#include "holang/parser.hpp"
#include "holang.hpp"
#include "holang/variable_table.hpp"
#include <iostream>
#include <map>
#include <vector>

using namespace std;
using namespace holang;

void exit_by_unsupported(const string &func) {
  cerr << func << " are not supported yet." << endl;
  exit(1);
}

void print_code(const vector<Code> &codes);

void print_offset(int offset) {
  for (int i = 0; i < offset; i++) {
    cout << ".  ";
  }
}

struct IntLiteralNode : public Node {
  int value;
  IntLiteralNode(int value) : value(value){};
  virtual void print(int offset) {
    print_offset(offset);
    cout << "IntLiteral " << value << "" << endl;
  }
  virtual void code_gen(vector<Code> *codes) {
    codes->push_back({.op = Instruction::PUT_INT});
    codes->push_back({.ival = value});
  }
};

constexpr const char *bool2s(const bool val) { return val ? "true" : "false"; }

struct BoolLiteralNode : public Node {
  bool value;
  BoolLiteralNode(bool value) : value(value){};
  virtual void print(int offset) {
    print_offset(offset);
    cout << "BoolLiteral " << bool2s(value) << endl;
  }
  virtual void code_gen(vector<Code> *codes) {
    codes->push_back({.op = Instruction::PUT_BOOL});
    codes->push_back({.bval = value});
  }
};

struct StringLiteralNode : public Node {
  string str;
  StringLiteralNode(const string &str) : str(str){};
  virtual void print(int offset) {
    print_offset(offset);
    cout << "StringLiteral \"" << str << "\"" << endl;
  }
  virtual void code_gen(vector<Code> *codes) {
    codes->push_back({.op = Instruction::PUT_STRING});
    codes->push_back({.sval = &str});
  }
};

struct IdentNode : public Node {
  string ident;
  int index;
  IdentNode(const string &ident, int index) : ident(ident), index(index) {}
  virtual void print(int offset) {
    print_offset(offset);
    cout << "Ident " << ident << " : " << index << endl;
  }
  virtual void code_gen(vector<Code> *codes) {
    codes->push_back({.op = Instruction::LOAD_LOCAL});
    codes->push_back({.ival = index});
  }
};

Instruction to_opcode(Keyword c) {
  switch (c) {
  case Keyword::ADD:
    return Instruction::ADD;
  case Keyword::SUB:
    return Instruction::SUB;
  case Keyword::MUL:
    return Instruction::MUL;
  case Keyword::DIV:
    return Instruction::DIV;
  case Keyword::LT:
    return Instruction::LESS;
  case Keyword::GT:
    return Instruction::GREATER;
  default:
    cerr << "err: to_opecode" << endl;
    exit(1);
  }
}

struct BinopNode : public Node {
  Keyword op;
  Node *lhs, *rhs;
  BinopNode(Keyword op, Node *lhs, Node *rhs) : op(op), lhs(lhs), rhs(rhs) {}
  virtual void print(int offset) {
    print_offset(offset);
    cout << "Binop " << op << endl;
    lhs->print(offset + 1);
    rhs->print(offset + 1);
  }
  virtual void code_gen(vector<Code> *codes) {
    lhs->code_gen(codes);
    rhs->code_gen(codes);
    codes->push_back({.op = to_opcode(op)});
  }
};

struct AssignNode : public Node {
  IdentNode *lhs;
  Node *rhs;
  AssignNode(IdentNode *ident, Node *rhs) : lhs(ident), rhs(rhs) {}
  virtual void print(int offset) {
    print_offset(offset);
    cout << "Assign " << lhs->ident << " : " << lhs->index << endl;
    rhs->print(offset + 1);
  }
  virtual void code_gen(vector<Code> *codes) {
    rhs->code_gen(codes);
    codes->push_back({.op = Instruction::STORE_LOCAL});
    codes->push_back({.ival = lhs->index});
  }
};

struct ExprsNode : public Node {
  Node *current;
  Node *next;
  int num;
  ExprsNode(Node *current, Node *next, int num)
      : current(current), next(next), num(num) {}
  virtual void print(int offset) {
    current->print(offset);
    next->print(offset);
  }
  virtual void code_gen(vector<Code> *codes) {
    current->code_gen(codes);
    next->code_gen(codes);
  }
};

struct StmtsNode : public Node {
  Node *current;
  Node *next;
  StmtsNode(Node *current, Node *next) : current(current), next(next) {}
  virtual void print(int offset) {
    current->print(offset);
    next->print(offset);
  }
  virtual void code_gen(vector<Code> *codes) {
    current->code_gen(codes);
    codes->push_back({Instruction::POP});
    next->code_gen(codes);
  }
};

struct IfNode : public Node {
  Node *cond, *then, *els;
  IfNode(Node *cond, Node *then, Node *els)
      : cond(cond), then(then), els(els) {}
  virtual void print(int offset) {
    print_offset(offset);
    cout << "if" << endl;
    cond->print(offset + 1);

    print_offset(offset);
    cout << "then " << endl;
    then->print(offset + 1);

    if (els != nullptr) {
      print_offset(offset);
      cout << "else" << endl;
      els->print(offset + 1);
    }
  }
  virtual void code_gen(vector<Code> *codes) {
    cond->code_gen(codes);

    int from_if = codes->size() + 1;
    codes->push_back({Instruction::JUMP_IFNOT});
    codes->push_back({.ival = 0}); // dummy
    then->code_gen(codes);

    int from_then = codes->size() + 1;
    codes->push_back({Instruction::JUMP});
    codes->push_back({.ival = 0}); // dummy

    int to_else = codes->size();
    if (els == nullptr) {
      // nilの概念ができたらnilにする
      codes->push_back({Instruction::PUT_INT});
      codes->push_back({.ival = 0});
    } else {
      els->code_gen(codes);
    }
    int to_end = codes->size();

    codes->at(from_if).ival = to_else;
    codes->at(from_then).ival = to_end;
  }
};

struct FuncCallNode : public Node {
  string name;
  vector<Node *> args;
  bool is_trailer;
  FuncCallNode(const string &name, const vector<Node *> &args, bool is_trailer)
      : name(name), args(args), is_trailer(is_trailer) {}
  virtual void print(int offset) {
    print_offset(offset);
    cout << "Call " << name << endl;
    for (const auto &arg : args) {
      arg->print(offset + 1);
    }
  }
  virtual void code_gen(vector<Code> *codes) {
    if (!is_trailer) {
      codes->push_back({Instruction::PUT_SELF});
    }

    for (Node *arg : args) {
      arg->code_gen(codes);
    }
    codes->push_back({Instruction::CALL_FUNC});
    codes->push_back({.sval = &name});
    codes->push_back({.ival = (int)args.size()});
  }
};

struct FuncDefNode : public Node {
  string name;
  vector<string *> params;
  Node *body;
  FuncDefNode(const string &name, const vector<string *> &params, Node *body)
      : name(name), params(params), body(body) {}
  virtual void print(int offset) {
    print_offset(offset);
    cout << "FuncDef " << name << endl;
    body->print(offset + 1);
  }
  virtual void code_gen(vector<Code> *codes) {
    vector<Code> body_code;

    body->code_gen(&body_code);
    body_code.push_back({Instruction::RET});
    codes->push_back({Instruction::DEF_FUNC});
    codes->push_back({.sval = &name});
    codes->push_back({.objval = (Object *)new Func(body_code)});
  }
};

struct KlassDefNode : public Node {
  string name;
  Node *body;
  KlassDefNode(const string &name, Node *body) : name(name), body(body) {}
  virtual void print(int offset) {
    print_offset(offset);
    cout << "KlassDef " << name << endl;
    body->print(offset + 1);
  }

  virtual void code_gen(vector<Code> *codes) {
    codes->push_back({Instruction::LOAD_CLASS});
    codes->push_back({.sval = &name});

    body->code_gen(codes);

    codes->push_back({Instruction::PREV_ENV});
  }
};

struct SignChangeNode : public Node {
  Node *body;
  SignChangeNode(Node *body) : body(body) {}
  virtual void print(int offset) {
    print_offset(offset);
    cout << "SignChange" << endl;
    body->print(offset + 1);
  }

  virtual void code_gen(vector<Code> *codes) {
    exit_by_unsupported("SignChangeNode#code_gen()");
  }
};

struct PrimeExprNode : public Node {
  Node *prime;
  Node *traier;
  PrimeExprNode(Node *prime, Node *traier) : prime(prime), traier(traier) {}
  virtual void print(int offset) {
    prime->print(offset);
    traier->print(offset + 1);
  }

  virtual void code_gen(vector<Code> *codes) {
    prime->code_gen(codes);
    traier->code_gen(codes);
  }
};

struct RefFieldNode : public Node {
  string *field;
  RefFieldNode(string *field) : field(field) {}
  virtual void print(int offset) {
    print_offset(offset);
    cout << "." << *field << endl;
  }

  virtual void code_gen(vector<Code> *codes) {
    codes->push_back({Instruction::LOAD_OBJ_FIELD});
    codes->push_back({.sval = field});
  }
};

struct SendNode : public Node {
  vector<Node *> args;
  Node *traier;
  SendNode(const vector<Node *> &args, Node *traier)
      : args(args), traier(traier) {}
  virtual void print(int offset) {
    print_offset(offset);
    cout << "Call" << endl;
    // TODO: print args
    traier->print(offset + 1);
  }

  virtual void code_gen(vector<Code> *codes) {
    for (Node *arg : args) {
      arg->code_gen(codes);
    }
    exit_by_unsupported("SendNode#code_gen()");
    exit(1);
  }
};

struct ImportNode : public Node {
  Node *module;
  ImportNode(Node *module) : module(module) {}
  virtual void print(int offset) {
    print_offset(offset);
    cout << "Import" << endl;
    module->print(offset + 1);
  }

  virtual void code_gen(vector<Code> *codes) {
    module->code_gen(codes);
    codes->push_back({Instruction::IMPORT});
  }
};

// ----- parser ----- //

Node *Parser::parse() { return read_toplevel(); }

void Parser::take(TokenType type) {
  Token *token = get();
  if (token->type != type) {
    exit_by_unexpected(type, token);
  }
}

void Parser::take(Keyword keyword) {
  Token *token = get();
  if (token->type != TKEYWORD || token->keyword != keyword) {
    exit_by_unexpected(keyword, token);
  }
}

void Parser::consume_newlines() {
  while (next_token(TNEWLINE)) {
  }
}

// ----- top level ----- //

Node *Parser::read_toplevel() {
  Node *root = nullptr;

  while (!is_eof()) {
    Node *node = read_stmt();
    consume_newlines();

    if (root != nullptr) {
      root = new StmtsNode(root, node);
    } else {
      root = node;
    }
  }
  return root;
}

Node *Parser::read_stmts() {
  Node *node = read_stmt();
  while (!(is_eof() || is_next(Keyword::BRACER))) {
    node = new StmtsNode(node, read_stmt());
  }
  return node;
}

// ----- statement ----- //

Node *Parser::read_stmt() {
  Node *node;
  if (is_next(Keyword::IF)) {
    node = read_if();
  } else if (is_next(Keyword::FUNC)) {
    node = read_funcdef();
  } else if (is_next(Keyword::CLASS)) {
    node = read_klassdef();
  } else if (is_next(Keyword::IMPORT)) {
    node = read_import();
  } else if (is_next(Keyword::BRACEL)) {
    node = read_suite();
  } else {
    node = read_expr();
  }
  return node;
}

Node *Parser::read_if() {
  take(Keyword::IF);
  Node *node = read_expr();
  Node *then = read_suite();
  Node *els = next_token(Keyword::ELSE) ? read_stmt() : nullptr;
  return new IfNode(node, then, els);
}

Node *Parser::read_funcdef() {
  take(Keyword::FUNC);
  Token *ident = get_ident();
  variable_table.next();

  take(Keyword::PARENL);
  vector<string *> params;
  read_params(&params);
  take(Keyword::PARENR);

  Node *body = read_suite();
  variable_table.prev();
  return new FuncDefNode(*ident->sval, params, body);
}

Node *Parser::read_klassdef() {
  take(Keyword::CLASS);
  Token *ident = get_ident();
  Node *body = read_suite();
  return new KlassDefNode(*ident->sval, body);
}

Node *Parser::read_import() {
  take(Keyword::IMPORT);
  Node *node = read_expr();
  return new ImportNode(node);
}

Node *Parser::read_suite() {
  Node *suite = nullptr;

  take(Keyword::BRACEL);
  consume_newlines();
  while (!is_next(Keyword::BRACER)) {
    Node *node = read_stmt();
    consume_newlines();

    if (suite != nullptr) {
      suite = new StmtsNode(suite, node);
    } else {
      suite = node;
    }
  }
  take(Keyword::BRACER);
  return suite;
}

// ----- expression ----- //

Node *Parser::read_expr() { return read_assignment_expr(); }

Node *Parser::read_assignment_expr() {
  Token *token = get();
  if (token->type == TIDENT && next_token(Keyword::ASSIGN)) {
    int index = variable_table.get(*token->sval);
    return new AssignNode(new IdentNode(*token->sval, index),
                          read_assignment_expr());
  }
  unget();
  return read_comp_expr();
}

Node *ast_binop(Keyword op, Node *lhs, Node *rhs) {
  return new BinopNode(op, lhs, rhs);
}

Node *Parser::read_comp_expr() {
  Node *node = read_additive_expr();
  if (next_token(Keyword::LT))
    return ast_binop(Keyword::LT, node, read_additive_expr());
  else if (next_token(Keyword::GT))
    return ast_binop(Keyword::GT, node, read_additive_expr());
  else
    return node;
}

Node *Parser::read_multiplicative_expr() {
  Node *node = read_factor();
  while (true) {
    if (next_token(Keyword::MUL))
      node = ast_binop(Keyword::MUL, node, read_factor());
    else if (next_token(Keyword::DIV))
      node = ast_binop(Keyword::DIV, node, read_factor());
    else
      return node;
  }
}

Node *Parser::read_additive_expr() {
  Node *node = read_multiplicative_expr();
  while (true) {
    if (next_token(Keyword::ADD))
      node = ast_binop(Keyword::ADD, node, read_multiplicative_expr());
    else if (next_token(Keyword::SUB))
      node = ast_binop(Keyword::SUB, node, read_multiplicative_expr());
    else
      return node;
  }
}

Node *Parser::read_factor() {
  if (next_token(Keyword::SUB)) {
    return new SignChangeNode(read_prime_expr());
  } else {
    return read_prime_expr();
  }
}

Node *Parser::read_prime_expr() {
  Node *node = read_prime();
  while (true) {
    Node *traier = read_traier();
    if (traier == nullptr) {
      break;
    }
    node = new PrimeExprNode(node, traier);
  }
  return node;
}

Node *Parser::read_traier() {
  if (next_token(Keyword::DOT)) {
    return read_name_or_funccall(true);
  }
  return nullptr;
}

// ----- prime ----- //

Node *Parser::read_prime() {
  if (is_next(TNUMBER)) {
    return read_number();
  } else if (is_next(TIDENT)) {
    return read_name_or_funccall(false);
  } else if (next_token(Keyword::TRUE)) {
    return new BoolLiteralNode(true);
  } else if (next_token(Keyword::FALSE)) {
    return new BoolLiteralNode(false);
  } else if (is_next(TSTRING)) {
    return read_string();
  }
  exit_by_unexpected("something prime", get());
  return nullptr;
}

Node *Parser::read_number() {
  Token *token = get();
  int val = stoi(*token->sval);
  return new IntLiteralNode(val);
}

Node *Parser::read_string() {
  Token *token = get();
  return new StringLiteralNode(*token->sval);
}

Node *Parser::read_name_or_funccall(bool is_trailer) {
  Token *ident = get();
  if (next_token(Keyword::PARENL)) {
    vector<Node *> args;
    if (!next_token(Keyword::PARENR)) {
      read_exprs(args);
      take(Keyword::PARENR);
    }
    return new FuncCallNode(*ident->sval, args, is_trailer);
  } else {
    if (is_trailer) {
      return new RefFieldNode(ident->sval);
    } else {
      int index = variable_table.get(*ident->sval);
      return new IdentNode(*ident->sval, index);
    }
  }
}

Node *Parser::read_block() {
  take(Keyword::BRACEL);
  Node *node = read_stmts();
  take(Keyword::BRACER);
  return node;
}

void Parser::read_exprs(vector<Node *> &args) {
  args.push_back(read_expr());
  while (next_token(Keyword::COMMA)) {
    args.push_back(read_expr());
  }
}

void Parser::read_arglist(vector<Node *> *arglist) {
  if (is_next(Keyword::PARENR)) {
    return;
  }

  Node *arg = read_expr();
  arglist->emplace_back(arg);
  while (next_token(Keyword::COMMA)) {
    arg = read_expr();
    arglist->emplace_back(arg);
  }
}

void Parser::read_params(vector<string *> *params) {
  if (!is_next(TIDENT)) {
    return;
  }

  Token *token = get_ident();
  params->emplace_back(token->sval);
  while (next_token(Keyword::COMMA)) {
    token = get_ident();
    params->emplace_back(token->sval);
  }
}
