#include <CContextFreeEval.h>
#include <CStrParse.h>
#include <vector>
#include <cmath>
#include <cstdlib>

using std::string;
using std::vector;
using std::cout;
using std::endl;

#define DEG_TO_RAD(a) (M_PI*(a)/180.0)
#define RAD_TO_DEG(a) (180.0*(a)/M_PI)

static CEvalOp power_op_         = { "^" , 6 };
static CEvalOp times_op_         = { "*" , 5 };
static CEvalOp divide_op_        = { "/" , 5 };
static CEvalOp modulus_op_       = { "%" , 5 };
static CEvalOp plus_op_          = { "+" , 4 };
static CEvalOp minus_op_         = { "-" , 4 };
static CEvalOp less_op_          = { "<" , 3 };
static CEvalOp less_equal_op_    = { "<=", 3 };
static CEvalOp greater_op_       = { ">" , 3 };
static CEvalOp greater_equal_op_ = { ">=", 3 };
static CEvalOp equals_op_        = { "==", 2 };
static CEvalOp not_equals_op_    = { "!=", 2 };
static CEvalOp and_op_           = { "&&", 1 };
static CEvalOp or_op_            = { "||", 0 };

CEval::
CEval() :
 last_op_   (NULL),
 force_real_(false),
 degrees_   (false),
 debug_     (false)
{
}

CEval::
CEval(const CEval &eval) :
 last_op_   (NULL),
 force_real_(eval.force_real_),
 degrees_   (eval.degrees_),
 debug_     (eval.debug_)
{
}

CEval::
~CEval()
{
  reset();
}

void
CEval::
reset()
{
  last_op_ = NULL;

  stack_.clear();
}

bool
CEval::
eval(const string &str, double *result)
{
  CEvalValueRef rvalue;

  if (! eval(str, rvalue))
    return false;

  *result = rvalue->toReal();

  return true;
}

bool
CEval::
eval(const string &str, CEvalValueRef &result)
{
  reset();

  CStrParse parse(str);

  return eval1(parse, result);
}

bool
CEval::
eval1(CStrParse &parse, CEvalValueRef &result)
{
  while (! parse.eof()) {
    parse.skipSpace();

    if (parse.eof()) break;

    // value
    if      (parse.isDigit() || parse.isChar('.')) {
      double real    = 0.0;
      int    integer = 0;
      bool   is_real = false;

      if (! parse.readNumber(real, integer, is_real))
        return false;

      if (! is_real && force_real_) {
        real    = integer;
        is_real = true;
      }

      if (is_real)
        pushValue(CEvalValueRef(new CEvalRealValue(real)));
      else
        pushValue(CEvalValueRef(new CEvalIntValue(integer)));
    }
    // operator
    else if (parse.isOneOf("+-*/%<>=!&|^")) {
      CEvalOp *op = readOp(parse);

      if (op == NULL)
        return false;

      if (hasOperator() || ! hasValue()) {
        if (op == &plus_op_ || op == &minus_op_) {
          CEvalValueRef result1;

          if (! eval1(parse, result1))
            return false;

          if (result1->getType() == CEVAL_VALUE_REAL) {
            double real = result1->toReal();

            if (op == &minus_op_) real = -real;

            pushValue(CEvalValueRef(new CEvalRealValue(real)));
          }
          else if (result1->getType() == CEVAL_VALUE_INTEGER) {
            int integer = result1->toInt();

            if (op == &minus_op_) integer = -integer;

            pushValue(CEvalValueRef(new CEvalIntValue(integer)));
          }
          else
            return false;
        }
        else
          return false;
      }
      else {
        if (checkLastOperator(op)) {
          if (! evalLastOperator())
            return false;
        }

        pushOperator(op);
      }
    }
    // bracketed expression
    else if (parse.isChar('(')) {
      int pos1 = parse.getPos();

      if (! parse.skipChar())
        return false;

      int depth = 1;

      while (! parse.eof()) {
        char c;

        if (! parse.readChar(&c))
          return false;

        if      (c == '(')
          ++depth;
        else if (c == ')') {
          --depth;

          if (depth == 0)
            break;
        }
      }

      if (depth != 0)
        return false;

      int pos2 = parse.getPos();

      string str1 = parse.getAt(pos1 + 1, pos2 - pos1 - 2);

      CEval eval1(*this);

      CEvalValueRef result1;

      if (! eval1.eval(str1, result1))
        return false;

      pushValue(result1);
    }
    // function
    else if (parse.isAlpha()) {
      string name;

      if (! parse.readIdentifier(name))
        return false;

      parse.skipSpace();

      if (! parse.isChar('('))
        return false;

      int pos1 = parse.getPos();

      if (! parse.skipChar())
        return false;

      int depth = 1;

      while (! parse.eof()) {
        char c;

        if (! parse.readChar(&c))
          return false;

        if      (c == '(')
          ++depth;
        else if (c == ')') {
          --depth;

          if (depth == 0)
            break;
        }
      }

      if (depth != 0)
        return false;

      int pos2 = parse.getPos();

      string str1 = parse.getAt(pos1 + 1, pos2 - pos1 - 2);

      vector<string> args;

      CStrParse parse1(str1);

      while (! parse1.eof()) {
        int pos1 = parse1.getPos();

        while (! parse1.eof() && ! parse1.isChar(','))
          parse1.skipChar();

        int pos2 = parse1.getPos();

        string arg = parse1.getAt(pos1, pos2 - pos1);

        args.push_back(arg);

        if (! parse1.eof() && parse1.isChar(','))
          parse1.skipChar();
      }

      uint num_args = args.size();

      vector<CEvalValueRef> arg_vals;

      for (uint i = 0; i < num_args; ++i) {
        CEval eval1(*this);

        CEvalValueRef arg_val;

        if (! eval1.eval(args[i], arg_val))
          return false;

        arg_vals.push_back(arg_val);
      }

      if      (name == "abs") {
        if (num_args != 1) return false;

        double result1 = fabs(arg_vals[0]->toReal());

        pushValue(CEvalValueRef(new CEvalRealValue(result1)));
      }
      else if (name == "acos") {
        if (num_args != 1) return false;

        double result1 = acos(arg_vals[0]->toReal());

        if (degrees_) result1 = RAD_TO_DEG(result1);

        pushValue(CEvalValueRef(new CEvalRealValue(result1)));
      }
      else if (name == "asin") {
        if (num_args != 1) return false;

        double result1 = asin(arg_vals[0]->toReal());

        if (degrees_) result1 = RAD_TO_DEG(result1);

        pushValue(CEvalValueRef(new CEvalRealValue(result1)));
      }
      else if (name == "atan") {
        if (num_args != 1) return false;

        double result1 = atan(arg_vals[0]->toReal());

        if (degrees_) result1 = RAD_TO_DEG(result1);

        pushValue(CEvalValueRef(new CEvalRealValue(result1)));
      }
      else if (name == "atan2") {
        if (num_args != 2) return false;

        double result1 = atan2(arg_vals[0]->toReal(), arg_vals[1]->toReal());

        if (degrees_) result1 = RAD_TO_DEG(result1);

        pushValue(CEvalValueRef(new CEvalRealValue(result1)));
      }
      else if (name == "ceil") {
        if (num_args != 1) return false;

        double result1 = ceil(arg_vals[0]->toReal());

        pushValue(CEvalValueRef(new CEvalRealValue(result1)));
      }
      else if (name == "cos") {
        if (num_args != 1) return false;

        double a = arg_vals[0]->toReal();

        if (degrees_) a = DEG_TO_RAD(a);

        double result1 = cos(a);

        pushValue(CEvalValueRef(new CEvalRealValue(result1)));
      }
      else if (name == "cosh") {
        if (num_args != 1) return false;

        double result1 = cosh(arg_vals[0]->toReal());

        pushValue(CEvalValueRef(new CEvalRealValue(result1)));
      }
      else if (name == "exp") {
        if (num_args != 1) return false;

        double result1 = exp(arg_vals[0]->toReal());

        pushValue(CEvalValueRef(new CEvalRealValue(result1)));
      }
      else if (name == "floor") {
        if (num_args != 1) return false;

        double result1 = floor(arg_vals[0]->toReal());

        pushValue(CEvalValueRef(new CEvalRealValue(result1)));
      }
      else if (name == "log") {
        if (num_args != 1) return false;

        double result1 = log(arg_vals[0]->toReal());

        pushValue(CEvalValueRef(new CEvalRealValue(result1)));
      }
      else if (name == "log10") {
        if (num_args != 1) return false;

        double result1 = log10(arg_vals[0]->toReal());

        pushValue(CEvalValueRef(new CEvalRealValue(result1)));
      }
      else if (name == "mod") {
        if (num_args != 2) return false;

        double result1 = fmod(arg_vals[0]->toReal(), arg_vals[1]->toReal());

        pushValue(CEvalValueRef(new CEvalRealValue(result1)));
      }
      else if (name == "pow") {
        if (num_args != 2) return false;

        double result1 = pow(arg_vals[0]->toReal(), arg_vals[1]->toReal());

        pushValue(CEvalValueRef(new CEvalRealValue(result1)));
      }
      else if (name == "rand_static") {
        double result1 = 0.0;

        if      (num_args == 0)
          result1 = randIn(0.0, 1.0);
        else if (num_args == 1) {
          double x = arg_vals[0]->toReal();

          if (x > 0)
            result1 = randIn(0.0, x);
          else
            result1 = randIn(x, 0.0);
        }
        else if (num_args == 2) {
          double x = arg_vals[0]->toReal();
          double y = arg_vals[1]->toReal();

          if (y > x)
            result1 = randIn(x, y);
          else
            result1 = randIn(y, x);
        }
        else
          return false;

        pushValue(CEvalValueRef(new CEvalRealValue(result1)));
      }
      else if (name == "sin") {
        if (num_args != 1) return false;

        double a = arg_vals[0]->toReal();

        if (degrees_) a = DEG_TO_RAD(a);

        double result1 = sin(a);

        pushValue(CEvalValueRef(new CEvalRealValue(result1)));
      }
      else if (name == "sinh") {
        if (num_args != 1) return false;

        double result1 = sinh(arg_vals[0]->toReal());

        pushValue(CEvalValueRef(new CEvalRealValue(result1)));
      }
      else if (name == "sqrt") {
        if (num_args != 1) return false;

        double result1 = sqrt(arg_vals[0]->toReal());

        pushValue(CEvalValueRef(new CEvalRealValue(result1)));
      }
      else if (name == "tan") {
        if (num_args != 1) return false;

        double a = arg_vals[0]->toReal();

        if (degrees_) a = DEG_TO_RAD(a);

        double result1 = tan(a);

        pushValue(CEvalValueRef(new CEvalRealValue(result1)));
      }
      else if (name == "tanh") {
        if (num_args != 1) return false;

        double result1 = tanh(arg_vals[0]->toReal());

        pushValue(CEvalValueRef(new CEvalRealValue(result1)));
      }
      else
        return false;
    }
    else
      return false;

    if (getDebug())
      printStack();
  }

  if (checkLastOperator(NULL)) {
    if (! evalLastOperator())
      return false;

    if (getDebug())
      printStack();
  }

  uint size = stack_.size();

  while (size > 1) {
    if (size < 3) return false;

    CEvalValueRef value1, value2;
    CEvalOp*      op;

    bool rc3 = popValue(value2);
    bool rc2 = popOperator(&op);
    bool rc1 = popValue(value1);

    if (! rc1 || ! rc2 || ! rc3) return false;

    CEvalValueRef value = evalOperator(value1, op, value2);

    pushValue(value);

    if (getDebug())
      printStack();

    size = stack_.size();
  }

  if (! popValue(result))
    return false;

  return true;
}

CEvalOp *
CEval::
readOp(CStrParse &parse)
{
  CEvalOp *op = NULL;

  char c;

  if (! parse.readChar(&c))
    return false;

  switch (c) {
    case '+': op = &plus_op_   ; break;
    case '-': op = &minus_op_  ; break;
    case '*': op = &times_op_  ; break;
    case '/': op = &divide_op_ ; break;
    case '%': op = &modulus_op_; break;
    case '^': op = &power_op_  ; break;
    case '<': {
      if (parse.isChar('=')) {
        parse.readChar(&c);

        op = &less_equal_op_;
      }
      else
        op = &less_op_;

      break;
    }
    case '>': {
      if (parse.isChar('=')) {
        parse.readChar(&c);

        op = &greater_equal_op_;
      }
      else
        op = &greater_op_;

      break;
    }
    case '=': {
      if (parse.isChar('=')) {
        parse.readChar(&c);

        op = &equals_op_;
      }
      else
        return false;

      break;
    }
    case '!': {
      if (parse.isChar('=')) {
        parse.readChar(&c);

        op = &not_equals_op_;
      }
      else
        return false;

      break;
    }
    case '&': {
      if (parse.isChar('&')) {
        parse.readChar(&c);

        op = &and_op_;
      }
      else
        return false;

      break;
    }
    case '|': {
      if (parse.isChar('|')) {
        parse.readChar(&c);

        op = &or_op_;
      }
      else
        return false;

      break;
    }
  }

  return op;
}

void
CEval::
pushValue(CEvalValueRef valueRef)
{
  stack_.push_back(valueRef);
}

bool
CEval::
popValue(CEvalValueRef &valueRef)
{
  if (! hasValue()) return false;

  valueRef = stack_.back();

  stack_.pop_back();

  return true;
}

void
CEval::
pushOperator(CEvalOp *op)
{
  stack_.push_back(CEvalValueRef(new CEvalOperatorValue(op)));

  last_op_ = op;
}

bool
CEval::
popOperator(CEvalOp **op1)
{
  if (! hasOperator())
    return false;

  CEvalOperatorValue *op = stack_.back().cast<CEvalOperatorValue>();

  *op1 = op->getOp();

  stack_.pop_back();

  updateLastOp();

  return true;
}

void
CEval::
updateLastOp()
{
  last_op_ = NULL;

  uint num = stack_.size();

  for (uint i = 0; i < num; ++i) {
    CEvalValueRef value = stack_[i];

    if (value->getType() == CEVAL_VALUE_OPERATOR)
      last_op_ = value.cast<CEvalOperatorValue>()->getOp();
  }
}

bool
CEval::
hasValue()
{
  return (! stack_.empty() && stack_.back()->isValue());
}

bool
CEval::
hasOperator()
{
  return (! stack_.empty() && stack_.back()->getType() == CEVAL_VALUE_OPERATOR);
}

CEvalValueRef
CEval::
evalOperator(CEvalValueRef value2)
{
  CEvalOp *op;

  bool rc1 = popOperator(&op);

  assert(rc1);

  CEvalValueRef value1;

  bool rc2 = popValue(value1);

  assert(rc2);

  return evalOperator(value1, op, value2);
}

CEvalValueRef
CEval::
evalOperator(CEvalValueRef value1, CEvalOp *op, CEvalValueRef value2)
{
  if (value1->getType() == CEVAL_VALUE_REAL || value2->getType() == CEVAL_VALUE_REAL) {
    double rvalue1 = value1->toReal();
    double rvalue2 = value2->toReal();

    int    ivalue = 0;
    double rvalue = 0.0;
    bool   is_int = false;

    if      (op == &times_op_        ) rvalue = rvalue1 *  rvalue2;
    else if (op == &divide_op_       ) rvalue = rvalue1 /  rvalue2;
    else if (op == &modulus_op_      ) rvalue = fmod(rvalue1, rvalue2);
    else if (op == &plus_op_         ) rvalue = rvalue1 +  rvalue2;
    else if (op == &minus_op_        ) rvalue = rvalue1 -  rvalue2;
    else if (op == &power_op_        ) rvalue = pow(rvalue1, rvalue2);
    else if (op == &less_op_         ) { ivalue = rvalue1 <  rvalue2; is_int = true; }
    else if (op == &less_equal_op_   ) { ivalue = rvalue1 <= rvalue2; is_int = true; }
    else if (op == &greater_op_      ) { ivalue = rvalue1 >  rvalue2; is_int = true; }
    else if (op == &greater_equal_op_) { ivalue = rvalue1 >= rvalue2; is_int = true; }
    else if (op == &equals_op_       ) { ivalue = rvalue1 == rvalue2; is_int = true; }
    else if (op == &not_equals_op_   ) { ivalue = rvalue1 != rvalue2; is_int = true; }
    else if (op == &and_op_          ) { ivalue = rvalue1 && rvalue2; is_int = true; }
    else if (op == &or_op_           ) { ivalue = rvalue1 || rvalue2; is_int = true; }
    else                               assert(false);

    if (is_int)
      return CEvalValueRef(new CEvalIntValue(ivalue));
    else
      return CEvalValueRef(new CEvalRealValue(rvalue));
  }
  else {
    int ivalue1 = value1->toInt();
    int ivalue2 = value2->toInt();

    int    ivalue = 0;
    double rvalue = 0;

    bool is_real = false;

    if      (op == &times_op_        ) ivalue = ivalue1 *  ivalue2;
    else if (op == &divide_op_       ) ivalue = ivalue1 /  ivalue2;
    else if (op == &modulus_op_      ) ivalue = ivalue1 %  ivalue2;
    else if (op == &plus_op_         ) ivalue = ivalue1 +  ivalue2;
    else if (op == &minus_op_        ) ivalue = ivalue1 -  ivalue2;
    else if (op == &less_op_         ) ivalue = ivalue1 <  ivalue2;
    else if (op == &less_equal_op_   ) ivalue = ivalue1 <= ivalue2;
    else if (op == &greater_op_      ) ivalue = ivalue1 >  ivalue2;
    else if (op == &greater_equal_op_) ivalue = ivalue1 >= ivalue2;
    else if (op == &equals_op_       ) ivalue = ivalue1 == ivalue2;
    else if (op == &not_equals_op_   ) ivalue = ivalue1 != ivalue2;
    else if (op == &and_op_          ) ivalue = ivalue1 && ivalue2;
    else if (op == &or_op_           ) ivalue = ivalue1 || ivalue2;
    else if (op == &power_op_        ) { rvalue = pow(ivalue1, ivalue2); is_real = true; }
    else                               assert(false);

    if (is_real)
      return CEvalValueRef(new CEvalRealValue(rvalue));
    else
      return CEvalValueRef(new CEvalIntValue(ivalue));
  }
}

bool
CEval::
checkLastOperator(CEvalOp *op)
{
  if (last_op_ == NULL) return false;
  if (op       == NULL) return true ;

  int prec1 = last_op_->precedence;
  int prec2 = op      ->precedence;

  return (prec2 <= prec1);
}

bool
CEval::
evalLastOperator()
{
  CEvalValueRef value1, value2;
  CEvalOp*      op;

  bool rc1 = popValue(value1);
  bool rc2 = popOperator(&op);
  bool rc3 = popValue(value2);

  if (! rc1 || ! rc2 || ! rc3) return false;

  CEvalValueRef value = evalOperator(value2, op, value1);

  pushValue(value);

  return true;
}

void
CEval::
printStack()
{
  uint num = stack_.size();

  for (uint i = 0; i < num; ++i) {
    stack_[i]->print();

    cout << " ";
  }

  cout << endl;
}

double
CEval::
randIn(double min_val, double max_val)
{
  return (max_val - min_val)*((1.0*rand())/RAND_MAX) + min_val;
}

//------

void
CEvalRealValue::
print()
{
  cout << value_;
}

//------

void
CEvalIntValue::
print()
{
  cout << value_;
}

//------

void
CEvalOperatorValue::
print()
{
  cout << op_->str;
}
