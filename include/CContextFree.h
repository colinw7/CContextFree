#ifndef CCONTEXT_FREE_H
#define CCONTEXT_FREE_H

#include <CHSVA.h>
#include <CMatrix2D.h>
#include <CBBox2D.h>
#include <vector>
#include <map>

class CFile;
class CStrParse;
class CContextFreeParse;

template<typename T>
class COptValT {
 private:
  T    value_;
  bool valid_;

 public:
  COptValT() : value_(), valid_(false) { }

  explicit COptValT(const T &value) : value_(value), valid_(true) { }

  COptValT(const COptValT &rhs) : value_(rhs.value_), valid_(rhs.valid_) { }

  COptValT &operator=(const COptValT &rhs) {
    value_ = rhs.value_; valid_ = rhs.valid_;
    return *this;
  }

  COptValT &operator=(const T &value) { setValue(value); return *this; }

  bool isValid() const { return getValid(); }

  bool getValid() const { return valid_; }

  const T &getValue() const { if (! valid_) assert(false); return value_; }
  T       &getValue()       { if (! valid_) assert(false); return value_; }

  const T &getValue(const T &def_value) const { return (valid_ ? value_ : def_value); }

  void setValue(const T &value) { value_ = value; valid_ = true; }

  void setInvalid() { valid_ = false; }
};

class CContextFreePath;

//---

class CContextFree {
 public:
  enum PathOp {
    MOVE_TO_PATH_OP,
    RMOVE_TO_PATH_OP,
    LINE_TO_PATH_OP,
    RLINE_TO_PATH_OP,
    ARC_TO_PATH_OP,
    RARC_TO_PATH_OP,
    CURVE_TO_PATH_OP,
    RCURVE_TO_PATH_OP,
    CLOSE_PATH_OP,
    STROKE_PATH_OP,
    FILL_PATH_OP,
    LOOP_PATH_OP,
    LOOP_PATH_LIST_OP,
    NO_PATH_OP
  };

 public:
  struct Adjustment {
    Adjustment() :
     x(), y(), z(), sx(), sy(), sz(), rotate(), flip(), skew_x(), skew_y(),
     hue(), saturation(), brightness(), alpha(),
     lhue(), lsaturation(), lbrightness(), lalpha() {
      thue = false; tsaturation = false; tbrightness = false; talpha = false;

      m.setIdentity();
    }

    void buildMatrix();

    COptValT<double> x, y, z;
    COptValT<double> sx, sy, sz;
    COptValT<double> rotate;
    COptValT<double> flip;
    COptValT<double> skew_x, skew_y;
    COptValT<double> hue , saturation , brightness , alpha;
    COptValT<double> lhue, lsaturation, lbrightness, lalpha;
    bool             thue, tsaturation, tbrightness, talpha;
    CMatrix2D        m;
  };

  struct Tile {
    bool             is_set;
    COptValT<double> x, y;
    bool             s_set;
    double           sx, sy;
    COptValT<double> rotate;
    COptValT<double> skew_x, skew_y;
    CMatrix2D        m;

    Tile() :
     is_set(false), x(), y(), rotate(), skew_x(), skew_y() {
      s_set = false; sx = 1.0; sy = 1.0;

      m.setIdentity();
    }

    void reset() {
      is_set = false;

      x.setInvalid();
      y.setInvalid();

      s_set = false; sx = 1.0; sy = 1.0;

      rotate.setInvalid();

      skew_x.setInvalid();
      skew_y.setInvalid();

      m.setIdentity();
    }
  };

  struct State {
    CHSVA     color;
    CHSVA     lcolor;
    double    z;
    double    sz;
    CMatrix2D m;

    State() :
     color(0.0, 0.0, 0.0, 1.0), lcolor(0.0, 0.0, 0.0, 1.0),
     z(0.0), sz(1.0), m() {
      m.setIdentity();
    }
  };

  struct PathParams {
    PathParams(const std::string &str1="") :
     str(str1) {
    }

    bool hasArg(const std::string &arg) const {
      return (str.find(arg) != std::string ::npos);
    }

    std::string str;
  };

  struct PathPoints {
    COptValT<double> x, y;
    COptValT<double> x1, y1, x2, y2;
    COptValT<double> rx, ry;
    COptValT<double> r;
    COptValT<double> width;
    PathParams       p;
    Adjustment       adj;

    PathPoints() :
     x(), y(), x1(), y1(), x2(), y2(), rx(), ry(), r(), p(), adj() {
    }
  };

  class Path;

  class PathPart {
   public:
    PathPart(PathOp op) : op_(op) { }

    virtual ~PathPart() { }

    virtual bool isMoveTo() const { return false; }

    virtual void expand(CContextFree *c, const State &state) = 0;

    virtual void exec(CContextFree *c, const State &state) = 0;

   protected:
    PathOp op_;
  };

  class MoveToPathPart : public PathPart {
   public:
    MoveToPathPart(const PathPoints &p) :
     PathPart(MOVE_TO_PATH_OP), x_(0.0), y_(0.0) {
      x_ = p.x.getValue(0.0);
      y_ = p.y.getValue(0.0);
    }

    bool isMoveTo() const { return true; }

    void expand(CContextFree *c, const State &state);

    void exec(CContextFree *c, const State &state);

   private:
    double x_, y_;
  };

  class LineToPathPart : public PathPart {
   public:
    LineToPathPart(const PathPoints &p) :
     PathPart(LINE_TO_PATH_OP), x_(0.0), y_(0.0) {
      x_ = p.x.getValue(0.0);
      y_ = p.y.getValue(0.0);
    }

    void expand(CContextFree *c, const State &state);

    void exec(CContextFree *c, const State &state);

   private:
    double x_, y_;
  };

  class ArcToPathPart : public PathPart {
   public:
    ArcToPathPart(const PathPoints &p) :
     PathPart(ARC_TO_PATH_OP), x_(0.0), y_(0.0), rx_(0.0), ry_(0.0), a_(0.0),
     fa_(0), fs_(1), cw_(false) {
      x_ = p.x.getValue(0.0);
      y_ = p.y.getValue(0.0);

      if (p.rx.isValid() && p.ry.isValid()) {
        rx_ = p.rx.getValue();
        ry_ = p.ry.getValue();
        a_  = p.r .getValue(0.0);
      }
      else if (p.r.isValid()) {
        rx_ = p.r.getValue();
        ry_ = rx_;
        a_  = 0.0;
      }
      else {
        rx_ = 1.0;
        ry_ = 1.0;
        a_  = 0.0;
      }

      if (p.p.hasArg("large")) fa_ = 1;

      if (p.p.hasArg("cw")) cw_ = true;
    }

    void expand(CContextFree *c, const State &state);

    void exec(CContextFree *c, const State &state);

   private:
    double x_, y_, rx_, ry_, a_;
    int    fa_, fs_;
    bool   cw_;
  };

  class CurveToPathPart : public PathPart {
   public:
    CurveToPathPart(const PathPoints &p) :
     PathPart(CURVE_TO_PATH_OP), x_(0.0), y_(0.0), x1_(0.0), y1_(0.0), x2_(0.0), y2_(0.0), n_(2) {
      x_ = p.x.getValue(0.0);
      y_ = p.y.getValue(0.0);

      x1_ = p.x1.getValue(0.0);
      y1_ = p.y1.getValue(0.0);

      if (p.x2.isValid() && p.y2.isValid()) {
        x2_ = p.x2.getValue();
        y2_ = p.y2.getValue();

        n_ = 3;
      }
    }

    void expand(CContextFree *c, const State &state);

    void exec(CContextFree *c, const State &state);

   private:
    double x_, y_, x1_, y1_, x2_, y2_;
    int    n_;
  };

  class ClosePathPart : public PathPart {
   public:
    ClosePathPart(const PathPoints &) :
     PathPart(CLOSE_PATH_OP) {
    }

    void expand(CContextFree *c, const State &state);

    void exec(CContextFree *c, const State &state);
  };

  class StrokePathPart : public PathPart {
   public:
    StrokePathPart(const PathPoints &p) :
     PathPart(STROKE_PATH_OP), adj_(p.adj) {
      w_ = p.width;
    }

    void expand(CContextFree *c, const State &state);

    void exec(CContextFree *c, const State &state);

   private:
    Adjustment       adj_;
    COptValT<double> w_;
  };

  class FillPathPart : public PathPart {
   public:
    FillPathPart(const PathPoints &p) :
     PathPart(FILL_PATH_OP), adj_(p.adj), evenodd_(false) {
      if (p.p.hasArg("evenodd")) evenodd_ = true;
    }

    void expand(CContextFree *c, const State &state);

    void exec(CContextFree *c, const State &state);

   private:
    Adjustment adj_;
    bool       evenodd_;
  };

  class LoopPathPart : public PathPart {
   public:
    LoopPathPart(int n, const Adjustment &adj, PathPart *part);

   ~LoopPathPart();

    void expand(CContextFree *c, const State &state);

    void exec(CContextFree *c, const State &state);

   private:
    int         n_;
    Adjustment  adj_;
    PathPart   *part_;
  };

  class LoopPathPartList : public PathPart {
   public:
    LoopPathPartList(int n, const Adjustment &adj);

   ~LoopPathPartList();

    void addPart(PathPart *part) { parts_.push_back(part); }

    void expand(CContextFree *c, const State &state);

    void exec(CContextFree *c, const State &state);

   private:
    using PartList = std::vector<PathPart *>;

    int        n_;
    Adjustment adj_;
    PartList   parts_;
  };

  class Rule;

  class Action {
   public:
    Action() { }

    virtual ~Action() { }

    virtual void exec(CContextFree *c, const State &state) = 0;

    virtual void expand(CContextFree *c, const State &state) = 0;
  };

  class SimpleAction : public Action {
   public:
    SimpleAction(const std::string &name, const Adjustment &adj) :
     Action(), name_(name), adj_(adj), rule_(NULL) {
    }

    const std::string &getName () const { return name_; }

    const Adjustment &getAdjustment() const { return adj_; }

    void exec(CContextFree *c, const State &state);

    void expand(CContextFree *c, const State &state);

   protected:
    std::string name_;
    Adjustment  adj_;
    Rule*       rule_ { nullptr };
  };

  class LoopAction : public Action {
   public:
    LoopAction(int n, const Adjustment &nadj, const std::string &name, const Adjustment &adj);

   ~LoopAction();

    int getLoopNum() const { return n_; }

    const Adjustment &getLoopAdjustment() const { return nadj_; }

    const std::string &getName () const { return name_; }

    const Adjustment &getAdjustment() const { return adj_; }

    void exec(CContextFree *c, const State &state);

    void expand(CContextFree *c, const State &state);

   private:
    int         n_    { 0 };
    Adjustment  nadj_;
    std::string name_;
    Adjustment  adj_;
    Rule*       rule_ { nullptr };
  };

  class ComplexLoopAction : public Action {
   public:
    ComplexLoopAction(int n, const Adjustment &nadj, Action *action);

   ~ComplexLoopAction();

    int getLoopNum() const { return n_; }

    const Adjustment &getLoopAdjustment() const { return nadj_; }

    Action *getAction() const { return action_; }

    void exec(CContextFree *c, const State &state);

    void expand(CContextFree *c, const State &state);

   private:
    int         n_;
    Adjustment  nadj_;
    Action     *action_;
  };

  class PathAction : public Action {
   public:
    PathAction(Path *path);

   ~PathAction();

    Path *getPath() const { return path_; }

    void addPart(PathPart *part);

    void exec(CContextFree *c, const State &state);

    void expand(CContextFree *c, const State &state);

   private:
    using PartList = std::vector<PathPart *>;

    Path     *path_;
    PartList  parts_;
  };

  class ActionList {
   public:
    ActionList(Rule *rule, double weight=1.0);

   ~ActionList();

    Rule *rule() const { return rule_; }

    double getWeight() const { return weight_; }

    void addAction(Action *action) {
      actions_.push_back(action);
    }

    void exec(CContextFree *c, const State &state);

    void expand(CContextFree *c, const State &state);

   private:
    using ActionArray = std::vector<Action *>;

    Rule        *rule_   { nullptr };
    double       weight_ { 1.0 };
    ActionArray  actions_;
  };

  class Rule {
   public:
    Rule(CContextFree *c, const std::string &id);

    virtual ~Rule() { }

    virtual bool isBasic() const { return false; }

    void addActionList(ActionList *actionList);

    virtual const std::string &getName() const { return id_; }

    virtual void expand(const State &state);

    virtual void exec(const State &state);

   protected:
    ActionList *getActionList();

   protected:
    using ActionListArray = std::vector<ActionList *>;

    CContextFree*   c_           { nullptr };
    std::string     id_;
    double          totalWeight_ { 0.0 };
    ActionListArray actionLists_;
  };

  class SquareRule : public Rule {
   public:
    SquareRule(CContextFree *c, const std::string &id);

    bool isBasic() const;

    void expand(const State &state);

    void exec(const State &state);
  };

  class CircleRule : public Rule {
   public:
    CircleRule(CContextFree *c, const std::string &id);

    bool isBasic() const;

    void expand(const State &state);

    void exec(const State &state);
  };

  class TriangleRule : public Rule {
   public:
    TriangleRule(CContextFree *c, const std::string &id);

    bool isBasic() const;

    void expand(const State &state);

    void exec(const State &state);
  };

  class RuleState {
   public:
    RuleState(Rule *rule, const State &state=State(), double area=0.0) :
     rule_(rule), state_(state), area_(area) {
      assert(rule_);
    }

    Rule *getRule() const { return rule_; }

    const State &getState() const { return state_; }

    void setState(const State &state) { state_ = state; }

    void expand();

    void exec() { rule_->exec(state_); }

    double getArea() const;

   private:
    Rule   *rule_;
    State   state_;
    double  area_;
  };

  class Path : public Rule {
   public:
    Path(CContextFree *c, const std::string &name);

    bool isBasic() const { return true; }

    void expand(const State &state);

    void exec(const State &state);
  };

 public:
  CContextFree();

  virtual ~CContextFree();

  void reset();

  void setMaxShapes(uint max_shapes) { max_shapes_ = max_shapes; }
  uint getMaxShapes() const { return max_shapes_; }

  uint getNumShapes() const { return num_shapes_; }

  void setMinSize(double min_size) { min_size_ = min_size; }

  void setPixelSize(double pixelSize) { pixelSize_ = pixelSize; }

  bool parse(const std::string &fileName);

  virtual void expand();

  virtual bool tick() { return true; }

  const CBBox2D &getBBox() const { return bbox_; }

  virtual void render();

  void renderAt(double x, double y);

  bool getTile(double *xmin, double *ymin, double *xmax, double *ymax);

  virtual void fillBackground(const CHSVA &hsva);

  virtual void fillSquare  (double x1, double y1, double x2, double y2, const CMatrix2D &m,
                            const CHSVA &color);
  virtual void fillCircle  (double x, double y, double r, const CMatrix2D &m, const CHSVA &color);
  virtual void fillTriangle(double x1, double y1, double x2, double y2, double x3, double y3,
                            const CMatrix2D &m, const CHSVA &color);

  virtual void pathInit   ();
  virtual void pathTerm   ();
  virtual void pathMoveTo (double x, double y);
  virtual void pathLineTo (double x, double y);
  virtual void pathCurveTo(double x, double y, double x1, double y1, double x2, double y2);
  virtual void pathClose  ();
  virtual void pathStroke (const CHSVA &color, const CMatrix2D &m, double w);
  virtual void pathFill   (const CHSVA &color, const CMatrix2D &m);

  static State adjustState(const State &state, const Adjustment &adj);

  static double adjustHueValue(double base, double delta,
                               double target=0.0, bool useTarget=false);
  static double adjustColorValue(double base, double delta,
                                 double target=0.0, bool useTarget=false);

  const CMatrix2D &getAdjustMatrix() const { return adjustMatrix_; }

  bool checkSizeLimit(const State &state);

  bool checkMaxShapes() const;

  static double randIn(double r1, double r2);

 private:
  bool parseFile(const std::string &fileName);

  bool parseComment1();
  bool parseComment2();
  bool parseComment3();

  bool parseStartShape();
  bool parseInclude();
  bool parseBackground();

  bool    parseTile();
  bool    parseSize();
  bool    parseRule();
  Action *parseAction();
  bool    parseAdjustment(Adjustment &adj);
  bool    parseAdjustmentValue(const std::string &name, Adjustment &adj, bool compose);
  bool    parsePath();

  PathPart *parsePathPart();

  PathPart *parsePathOpPart(PathOp pathOp);

  PathOp lookupPathOp(const std::string &name);

  bool parsePathPoints(PathOp pathOp, PathPoints &points);

  bool parsePathValue(PathOp pathOp, const std::string &name, PathPoints &points);

  bool parseBlock1(std::string &str);

  bool parseName(std::string &name);

  bool parseColorValue(COptValT<double> &r, bool *target);

  bool isReal();

  bool parseReal(COptValT<double> &r);

  bool parseReal(double *r);

  bool parseRealValue(double *r);

  void skipSpace();

  bool skipComment();

  void bufferShapes();

  const std::string &getStartShape() const { return start_shape_; }

  void setStartShape(const std::string &name);

  Rule *getRule(const std::string &id);
  Path *getPath(const std::string &id);

  void pushRule(Rule *rule, const State &state);

  void dumpRuleStack();

  void bufferRule(double z, Rule *rule, const State &state, const CBBox2D &bbox);

  void incNumShapes() { ++num_shapes_; }

  CContextFreePath *getPath() { return path_; }

  void updateBBox(const CBBox2D &bbox);

  void error(const std::string &msg) const;

 private:
  using StringArray    = std::vector<std::string>;
  using RuleMap        = std::map<std::string, Rule *>;
  using RuleStateStack = std::vector<RuleState>;
  using ZRuleStack     = std::map<int, RuleStateStack>;

  CContextFreeParse *parse_       { nullptr };
  std::string        start_shape_;
  CHSVA              bg_          { 0, 0, 0 };
  Tile               tile_;
  RuleMap            rules_;
  RuleStateStack     ruleStack_;
  StringArray        includes_;
  uint               num_shapes_ { 0 };
  uint               max_shapes_ { 500000 };
  double             min_size_   { 0.3 };
  ZRuleStack         zRuleStack_;
  double             pixelSize_  { 1.0 };
  CContextFreePath  *path_       { nullptr };
  CBBox2D            bbox_;
  CMatrix2D          adjustMatrix_;
};

//-------------

class CContextFreePathPart {
 public:
  CContextFreePathPart() { }

  virtual ~CContextFreePathPart() { }

  virtual void updateBBox(CContextFree *c, const CContextFree::State &state, CBBox2D &bbox) = 0;

  virtual void addToPath(CContextFree *c) = 0;
};

class CContextFreePathMoveTo : public CContextFreePathPart {
 public:
  CContextFreePathMoveTo(double x, double y);

  void updateBBox(CContextFree *c, const CContextFree::State &state, CBBox2D &bbox);

  void addToPath(CContextFree *c);

 private:
  double x_, y_;
};

class CContextFreePathLineTo : public CContextFreePathPart {
 public:
  CContextFreePathLineTo(double x, double y);

  void updateBBox(CContextFree *c, const CContextFree::State &state, CBBox2D &bbox);

  void addToPath(CContextFree *c);

 private:
  double x_, y_;
};

class CContextFreePathCurve3To : public CContextFreePathPart {
 public:
  CContextFreePathCurve3To(double x2, double y2, double x3, double y3, double x4, double y4);

  void updateBBox(CContextFree *c, const CContextFree::State &state, CBBox2D &bbox);

  void addToPath(CContextFree *c);

 private:
  double x2_, y2_, x3_, y3_, x4_, y4_;
};

class CContextFreePathClose : public CContextFreePathPart {
 public:
  CContextFreePathClose();

  void updateBBox(CContextFree *c, const CContextFree::State &state, CBBox2D &bbox);

  void addToPath(CContextFree *c);
};

class CContextFreePath {
 public:
  CContextFreePath();
 ~CContextFreePath();

  void clear();

  void clearParts();

  void reset();

  void addPart(CContextFreePathPart *part);

  void setClosed(bool closed) { closed_ = closed; }

  void strokeBBox(CContextFree *c, const CContextFree::State &state, double w, CBBox2D &bbox);

  void stroke(CContextFree *c, const CContextFree::State &state, double w);

  void fillBBox(CContextFree *c, const CContextFree::State &state, CBBox2D &bbox);

  void fill(CContextFree *c, const CContextFree::State &state);

  void term(CContextFree *c);

  bool getStroked() const { return stroked_; }
  bool getFilled () const { return filled_ ; }

  void setCurrentPoint(double x, double y);
  void resetCurrentPoint();
  bool getCurrentPoint(double *x, double *y) const;
  bool getCurrentPointSet() const { return currentSet_; }

  void resetBBox() { bbox_.reset(); }

  void updateBBox(const CBBox2D &bbox);

  const CBBox2D &getBBox() const { return bbox_; }

 private:
  using PartList = std::vector<CContextFreePathPart *>;

  PartList parts_;
  bool     closed_;
  bool     stroked_;
  bool     filled_;
  double   currentX_, currentY_;
  bool     currentSet_;
  CBBox2D  bbox_;
};

#endif
