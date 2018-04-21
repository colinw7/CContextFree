#ifndef CEVAL_H
#define CEVAL_H

#include <CRefPtr.h>
#include <deque>

class CStrParse;

struct CEvalOp {
  const char *str;
  int         precedence;
};

class CEvalValue;

typedef CRefPtr<CEvalValue> CEvalValueRef;

enum CEvalValueType {
  CEVAL_VALUE_REAL,
  CEVAL_VALUE_INTEGER,
  CEVAL_VALUE_OPERATOR
};

class CEvalValue {
 public:
  CEvalValue(CEvalValueType type) :
   type_(type) {
  }

  virtual ~CEvalValue() { }

  CEvalValueType getType() const { return type_; }

  virtual bool isValue() const { return false; }

  virtual double toReal() const { return 0.0; }
  virtual int    toInt () const { return 0  ; }

  virtual void print() { }

 protected:
  CEvalValueType type_;
};

class CEvalRealValue : public CEvalValue {
 public:
  CEvalRealValue(double value) :
   CEvalValue(CEVAL_VALUE_REAL), value_(value) {
  }

  bool isValue() const { return true; }

  double getValue() const { return value_; }

  double toReal() const { return value_; }
  int    toInt () const { return int(value_); }

  void print();

 protected:
  double value_;
};

class CEvalIntValue : public CEvalValue {
 public:
  CEvalIntValue(int value) :
   CEvalValue(CEVAL_VALUE_INTEGER), value_(value) {
  }

  bool isValue() const { return true; }

  int getValue() const { return value_; }

  double toReal() const { return double(value_); }
  int    toInt () const { return value_; }

  void print();

 protected:
  int value_;
};

class CEvalOperatorValue : public CEvalValue {
 public:
  CEvalOperatorValue(CEvalOp *op) :
   CEvalValue(CEVAL_VALUE_OPERATOR), op_(op) {
  }

  CEvalOp *getOp() const { return op_; }

  void print();

 protected:
  CEvalOp *op_;
};

class CEval {
 public:
  CEval();

  virtual ~CEval();

  void setDebug(bool debug=true) { debug_ = debug; }

  bool getDebug() const { return debug_; }

  void setForceReal(bool force_real=true) { force_real_ = force_real; }

  void setDegrees(bool degrees=true) { degrees_ = degrees; }

  bool eval(const std::string &str, double *result);

 protected:
  CEval(const CEval &eval);

  void reset();

  CEvalOp *readOp(CStrParse &parse);

  void pushValue(CEvalValueRef valueRef);

  bool popValue(CEvalValueRef &value);

  void pushOperator(CEvalOp *op);
  bool popOperator(CEvalOp **op);

  void updateLastOp();

  bool hasValue();
  bool hasOperator();

  bool eval(const std::string &str, CEvalValueRef &result);

  bool eval1(CStrParse &parse, CEvalValueRef &result);

  CEvalValueRef evalOperator(CEvalValueRef value2);
  CEvalValueRef evalOperator(CEvalValueRef value1, CEvalOp *op, CEvalValueRef value2);

  bool checkLastOperator(CEvalOp *op);

  bool evalLastOperator();

  void printStack();

  double randIn(double min_val, double max_val);

 protected:
  typedef std::deque<CEvalValueRef> ValueStack;

  ValueStack  stack_;
  CEvalOp    *last_op_;
  bool        force_real_;
  bool        degrees_;
  bool        debug_;
};

#endif
