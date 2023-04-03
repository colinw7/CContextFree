#ifndef CEVAL_H
#define CEVAL_H

// NOTE: copy of CEval.h

#include <CRefPtr.h>
#include <deque>

class CStrParse;

struct CEvalOp {
  const char *str;
  int         precedence;
};

class CEvalValue;

using CEvalValueRef = CRefPtr<CEvalValue>;

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

//---

class CEvalRealValue : public CEvalValue {
 public:
  CEvalRealValue(double value) :
   CEvalValue(CEVAL_VALUE_REAL), value_(value) {
  }

  bool isValue() const override { return true; }

  double getValue() const { return value_; }

  double toReal() const override { return value_; }
  int    toInt () const override { return int(value_); }

  void print() override;

 protected:
  double value_;
};

//---

class CEvalIntValue : public CEvalValue {
 public:
  CEvalIntValue(int value) :
   CEvalValue(CEVAL_VALUE_INTEGER), value_(value) {
  }

  bool isValue() const override { return true; }

  int getValue() const { return value_; }

  double toReal() const override { return double(value_); }
  int    toInt () const override { return value_; }

  void print() override;

 protected:
  int value_;
};

//---

class CEvalOperatorValue : public CEvalValue {
 public:
  CEvalOperatorValue(CEvalOp *op) :
   CEvalValue(CEVAL_VALUE_OPERATOR), op_(op) {
  }

  CEvalOp *getOp() const { return op_; }

  void print() override;

 protected:
  CEvalOp *op_;
};

//---

class CEval {
 public:
  CEval();

  virtual ~CEval();

  virtual bool handleUnknown(CStrParse &) { return false; }

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
  using ValueStack = std::deque<CEvalValueRef>;

  ValueStack stack_;
  CEvalOp*   last_op_    { nullptr };
  bool       force_real_ { false };
  bool       degrees_    { false };
  bool       debug_      { false };
};

#endif
