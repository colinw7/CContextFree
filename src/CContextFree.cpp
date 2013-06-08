#include <CContextFree.h>
#include <CContextFreeEval.h>
#include <CFile.h>
#include <CMathGeom2D.h>
#include <CArcToBezier.h>
#include <C3Bezier2D.h>
#include <CStrParse.h>
#include <algorithm>

using std::string;
using std::vector;
using std::cerr;
using std::endl;

class CContextFreeParse : public CStrParse {
 public:
  CContextFreeParse(CContextFree *c, const std::string &filename);
 ~CContextFreeParse();

  bool eol() const;
  bool eof() const;

 private:
  bool fillBuffer();

 private:
  CContextFree *c_;
  CFile        *file_;
};

class CContextFreeCmp {
 public:
  CContextFreeCmp() {
  }

  int operator()(const CContextFree::RuleState &rule1, const CContextFree::RuleState &rule2) {
    return (rule1.getArea() > rule2.getArea());
  }
};

//------

CContextFree::
CContextFree() :
 parse_      (NULL),
 start_shape_(""),
 bg_         (0,0,1,1),
 tile_       (),
 rules_      (),
 ruleStack_  (),
 includes_   (),
 num_shapes_ (0),
 max_shapes_ (500000),
 min_size_   (0.3),
 zRuleStack_ (),
 pixelSize_  (1.0),
 path_       (NULL),
 bbox_       ()
{
  path_ = new CContextFreePath;

  adjustMatrix_.setIdentity();
}

CContextFree::
~CContextFree()
{
  reset();
}

void
CContextFree::
reset()
{
  delete parse_; parse_ = NULL;

  start_shape_ = "";

  bg_ = CHSVA(0,0,1,1);

  tile_.reset();

  RuleMap::iterator p1, p2;

  for (p1 = rules_.begin(), p2 = rules_.end(); p1 != p2; ++p1)
    delete (*p1).second;

  rules_.clear();

  ruleStack_.clear();

  includes_.clear();

  zRuleStack_.clear();

  delete path_;

  path_ = new CContextFreePath;

  bbox_.reset();

  adjustMatrix_.setIdentity();
}

bool
CContextFree::
parse(const std::string &filename)
{
  start_shape_ = "";

  if (! parseFile(filename))
    return false;

  //------

  uint num_includes = includes_.size();

  for (uint i = 0; i < num_includes; ++i)
    parseFile(includes_[i]);

  return true;
}

bool
CContextFree::
parseFile(const std::string &filename)
{
  delete parse_;

  parse_ = new CContextFreeParse(this, filename);

  while (! parse_->eof()) {
    skipSpace();

    if (parse_->eol()) continue;

    if (parse_->isIdentifier()) {
      std::string id;

      if (! parseName(id)) return false;

      if      (id == "startshape") {
        parseStartShape();
      }
      else if (id == "include") {
        parseInclude();
      }
      else if (id == "background") {
        parseBackground();
      }
      else if (id == "tile") {
        parseTile();
      }
      else if (id == "size") {
        parseSize();
      }
      else if (id == "rule") {
        parseRule();
      }
      else if (id == "path") {
        parsePath();
      }
      else {
        error("Bad token");
        return false;
      }
    }
    else {
      error("Bad token");
      return false;
    }
  }

  return true;
}

bool
CContextFree::
getTile(double *xmin, double *ymin, double *xmax, double *ymax)
{
  if (! tile_.is_set) return false;

  double x1, y1, x2, y2;

  tile_.m.multiplyPoint(-0.5, -0.5, &x1, &y1);
  tile_.m.multiplyPoint( 0.5,  0.5, &x2, &y2);

  *xmin = std::min(x1, x2);
  *ymin = std::min(y1, y2);
  *xmax = std::max(x1, x2);
  *ymax = std::max(y1, y2);

  return true;
}

void
CContextFree::
expand()
{
  srand(time(NULL));

  num_shapes_ = 0;

  const std::string &name = getStartShape();

  Rule *rule = getRule(name);

  if (rule == NULL) {
    error("No start shape : " + name);
    return;
  }

  zRuleStack_.clear();

  RuleStateStack ruleStack;

  ruleStack_.clear();

  pushRule(rule, State());

  while (! ruleStack_.empty()) {
    std::swap(ruleStack, ruleStack_);

    ruleStack_.clear();

    uint n = ruleStack.size();

    for (uint i = 0; i < n; ++i) {
      RuleState &ruleState = ruleStack[i];

      ruleState.expand();
    }

    if (! tick()) break;
  }
cerr << getNumShapes() << endl;

cerr << bbox_ << endl;
}

void
CContextFree::
updateBBox(const CBBox2D &bbox)
{
  bbox_.add(bbox);
}

void
CContextFree::
render()
{
  adjustMatrix_.setIdentity();

  fillBackground(bg_);

  double xmin, ymin, xmax, ymax;

  if (getTile(&xmin, &ymin, &xmax, &ymax)) {
    double w = fabs(xmax - xmin);
    double h = fabs(ymax - ymin);

    int nl = 0; while (xmin > bbox_.getXMin()) { xmin -= w; ++nl; }
    int nr = 0; while (xmax < bbox_.getXMax()) { xmax += w; ++nr; }
    int nb = 0; while (ymin > bbox_.getYMin()) { ymin -= h; ++nb; }
    int nt = 0; while (ymax < bbox_.getYMax()) { ymax += h; ++nt; }

    int nx = nl + nr + 1;
    int ny = nb + nt + 1;

    for (int iy = 0; iy < ny; ++iy) {
      double y = -(iy - nb)*h;

      for (int ix = 0; ix < nx; ++ix) {
        double x = -(ix - nl)*w;

        renderAt(x, y);
      }
    }
  }
  else {
    ZRuleStack::iterator p1, p2;

    for (p1 = zRuleStack_.begin(), p2 = zRuleStack_.end(); p1 != p2; ++p1) {
      RuleStateStack &ruleStack = (*p1).second;

      std::sort(ruleStack.begin(), ruleStack.end(), CContextFreeCmp());

      uint num = ruleStack.size();

      for (uint i = 0; i < num; ++i) {
        RuleState &ruleState = ruleStack[i];

        ruleState.exec();
      }
    }
  }
}

void
CContextFree::
renderAt(double x, double y)
{
  adjustMatrix_ = CMatrix2D::translation(x, y);

  ZRuleStack::iterator p1, p2;

  for (p1 = zRuleStack_.begin(), p2 = zRuleStack_.end(); p1 != p2; ++p1) {
    RuleStateStack &ruleStack = (*p1).second;

    uint num = ruleStack.size();

    for (uint i = 0; i < num; ++i) {
      RuleState &ruleState = ruleStack[i];

      ruleState.exec();
    }
  }
}

bool
CContextFree::
parseComment1()
{
  parse_->skipChars(2);

  while (! parse_->eof() && (! parse_->isChar('*') || ! parse_->isNextChar('/')))
    parse_->skipChar();

  parse_->skipChars(2);

  return true;
}

bool
CContextFree::
parseComment2()
{
  parse_->skipChars(2);

  while (! parse_->eol())
    parse_->skipChar();

  return true;
}

bool
CContextFree::
parseComment3()
{
  parse_->skipChar();

  while (! parse_->eol())
    parse_->skipChar();

  return true;
}

bool
CContextFree::
parseStartShape()
{
  // start_shape <user_string>
  if (! parse_->isIdentifier())
    return false;

  std::string id;

  if (! parseName(id)) return false;

  setStartShape(id);

  return true;
}

bool
CContextFree::
parseInclude()
{
  // include <user_string>
  // include <user_filename>
  std::string filename;

  if (parse_->isChar('"')) {
    if (! parse_->readString(filename))
      return false;
  }
  else {
    if (! parse_->readNonSpace(filename))
      return false;
  }

  includes_.push_back(filename);

  skipSpace();

  return true;
}

bool
CContextFree::
parseBackground()
{
  // background { <color_adjustments> }
  if (! parse_->isChar('{') && ! parse_->isChar('['))
    return false;

  char end_char = (parse_->isChar('{') ? '}' : ']');

  parse_->skipChar();

  skipSpace();

  double hue = 0.0, saturation = 0.0, brightness = 0.0, alpha = 0.0;

  while (! parse_->eof() && ! parse_->isChar(end_char)) {
    std::string name;

    if (! parseName(name)) return false;

    if      (name == "hue" || name == "h") {
      if (! parseReal(&hue)) hue = 0.0;

      while (hue <    0.0) hue += 360.0;
      while (hue >= 360.0) hue -= 360.0;
    }
    else if (name == "saturation" || name == "sat") {
      if (! parseReal(&saturation)) saturation = 1.0;

      saturation = std::min(std::max(saturation, -1.0), 1.0);
    }
    else if (name == "brightness" || name == "b") {
      if (! parseReal(&brightness)) brightness = 1.0;

      brightness = std::min(std::max(brightness, -1.0), 1.0);
    }
    else if (name == "alpha" || name == "a") {
      if (! parseReal(&alpha)) alpha = 1.0;

      alpha = std::min(std::max(alpha, -1.0), 1.0);
    }
    else {
      error("Invalid background value " + name);
    }

    skipSpace();
  }

  if (! parse_->eof() && parse_->isChar(end_char)) {
    parse_->skipChar();

    skipSpace();
  }

  bg_ += CHSVA(hue, saturation, brightness, alpha);

  return true;
}

bool
CContextFree::
parseTile()
{
  // tile <modification>
  if (! parse_->isChar('{') && ! parse_->isChar('['))
    return false;

  char end_char = (parse_->isChar('{') ? '}' : ']');

  parse_->skipChar();

  skipSpace();

  tile_.is_set = true;

  while (! parse_->eof() && ! parse_->isChar(end_char)) {
    std::string name;

    if (! parseName(name)) return false;

    if      (name == "size" || name == "s") {
      tile_.s_set = true;

      if (! parseReal(&tile_.sx)) tile_.sx = 1.0;

      tile_.sy = tile_.sx;

      skipSpace();

      if (isReal()) {
        if (! parseReal(&tile_.sy)) tile_.sy = 1.0;

        skipSpace();
      }
    }
    else if (name == "rotate" || name == "r") {
      if (! parseReal(tile_.rotate)) return false;
    }
    else if (name == "skew") {
      if (! parseReal(tile_.skew_x)) return false;
      if (! parseReal(tile_.skew_y)) return false;
    }
    else if (name == "x") {
      if (! parseReal(tile_.x)) return false;
    }
    else if (name == "y") {
      if (! parseReal(tile_.y)) return false;
    }
    else {
      error("Invalid tile " + name);
    }

    skipSpace();
  }

  if (! parse_->eof() && parse_->isChar(end_char)) {
    parse_->skipChar();

    skipSpace();
  }

  // set matrix
  if (tile_.x.isValid() || tile_.y.isValid()) {
    tile_.m *= CMatrix2D::translation(tile_.x.getValue(0.0), tile_.y.getValue(0.0));
  }

  if (tile_.rotate.isValid()) {
    double rrotate = M_PI*tile_.rotate.getValue()/180.0;

    tile_.m *= CMatrix2D::rotation(rrotate);
  }

  if (tile_.skew_x.isValid() || tile_.skew_y.isValid()) {
    double rskewx = M_PI*tile_.skew_x.getValue(0.0)/180.0;
    double rskewy = M_PI*tile_.skew_y.getValue(0.0)/180.0;

    tile_.m *= CMatrix2D::skew(rskewx, rskewy);
  }

  if (tile_.s_set) {
    tile_.m *= CMatrix2D::scale(tile_.sx, tile_.sy);
  }

  return true;
}

bool
CContextFree::
parseSize()
{
  // size <modification>
  if (! parse_->isChar('{') && ! parse_->isChar('['))
    return false;

  char end_char = (parse_->isChar('{') ? '}' : ']');

  double x = 0.0, y = 0.0;
  double w = 0.0, h = 0.0;

  parse_->skipChar();

  skipSpace();

  while (! parse_->eof() && ! parse_->isChar(end_char)) {
    std::string name;

    if (! parseName(name))  return false;

    if      (name == "x") {
      if (! parseReal(&x)) x = 0.0;
    }
    else if (name == "y") {
      if (! parseReal(&y)) y = 0.0;
    }
    else if (name == "size" || name == "s") {
      if (! parseReal(&w)) w = 1.0;

      h = w;

      if (isReal()) {
        if (! parseReal(&h)) h = 1.0;
      }
    }
    else {
      error("Invalid size parameter " + name);
    }

    skipSpace();
  }

  if (! parse_->eof() && parse_->isChar(end_char)) {
    parse_->skipChar();

    skipSpace();
  }

  return true;
}

bool
CContextFree::
parseRule()
{
  // rule <user_string> { <replacements> }
  // rule <user_string> <user_rational> { replacements }
  std::string id;

  parseName(id);

  skipSpace();

  Rule *rule = getRule(id);

  double weight = 1.0;

  if (isReal()) {
    if (! parseReal(&weight))
      return false;

    skipSpace();
  }

  if (! parse_->isChar('{') && ! parse_->isChar('['))
    return false;

  char end_char = (parse_->isChar('{') ? '}' : ']');

  parse_->skipChar();

  skipSpace();

  ActionList *actionList = new ActionList(rule, weight);

  while (! parse_->eof() && ! parse_->isChar(end_char)) {
    Action *action = parseAction();

    if (! action) return false;

    actionList->addAction(action);
  }

  if (! parse_->eof() && parse_->isChar(end_char)) {
    parse_->skipChar();

    skipSpace();
  }

  rule->addActionList(actionList);

  return true;
}


CContextFree::Action *
CContextFree::
parseAction()
{
  Action *action = NULL;

  if (parse_->isDigit()) {
    int n;

    if (! parse_->readInteger(&n)) return NULL;

    skipSpace();

    if (! parse_->isChar('*')) return NULL;

    parse_->skipChar();

    skipSpace();

    Adjustment nadj;

    if (! parseAdjustment(nadj)) return NULL;

    if (parse_->isChar('{') || parse_->isChar('[')) {
      char end_char = (parse_->isChar('{') ? '}' : ']');

      parse_->skipChar();

      skipSpace();

      Action *action1 = parseAction();

      if (! action1) return NULL;

      action = new ComplexLoopAction(n, nadj, action1);

      if (! parse_->eof() && parse_->isChar(end_char)) {
        parse_->skipChar();

        skipSpace();
      }
    }
    else {
      std::string name;

      if (! parseName(name)) return NULL;

      Adjustment adj;

      if (! parseAdjustment(adj)) return NULL;

      action = new LoopAction(n, nadj, name, adj);
    }
  }
  else {
    std::string name;

    if (! parseName(name)) return NULL;

    Adjustment adj;

    if (! parseAdjustment(adj)) return NULL;

    action = new SimpleAction(name, adj);
  }

  return action;
}

bool
CContextFree::
parseAdjustment(Adjustment &adj)
{
  if (parse_->isChar('{') || parse_->isChar('[')) {
    char end_char = (parse_->isChar('{') ? '}' : ']');

    bool compose = (end_char == ']');

    parse_->skipChar();

    skipSpace();

    while (! parse_->eof() && ! parse_->isChar(end_char)) {
      std::string name;

      if (! parseName(name)) return false;

      if (! parseAdjustmentValue(name, adj, compose))
        error("Invalid adjustment " + name);
    }

    if (! parse_->eof() && parse_->isChar(end_char)) {
      parse_->skipChar();

      skipSpace();
    }

    if (! compose)
      adj.buildMatrix();

    return true;
  }
  else {
    return false;
  }
}

bool
CContextFree::
parseAdjustmentValue(const std::string &name, Adjustment &adj, bool compose)
{
  if      (name == "x") {
    if (! parseReal(adj.x)) return false;

    if (compose)
      adj.m *= CMatrix2D::translation(adj.x.getValue(), 0.0);
  }
  else if (name == "y") {
    if (! parseReal(adj.y)) return false;

    if (compose)
      adj.m *= CMatrix2D::translation(0.0, adj.y.getValue());
  }
  else if (name == "z") {
    if (! parseReal(adj.z)) return false;
  }
  else if (name == "size" || name == "s") {
    if (! parseReal(adj.sx)) return false;

    adj.sy = adj.sx;
    adj.sz = adj.sx;

    skipSpace();

    if (isReal()) {
      if (! parseReal(adj.sy)) return false;

      adj.sz = adj.sy;

      skipSpace();

      if (isReal()) {
        if (! parseReal(adj.sz)) return false;

        skipSpace();
      }
    }

    if (compose)
      adj.m *= CMatrix2D::scale(adj.sx.getValue(), adj.sy.getValue());
  }
  else if (name == "rotate" || name == "r") {
    if (! parseReal(adj.rotate)) return false;

    if (compose) {
      double rrotate = M_PI*adj.rotate.getValue()/180.0;

      adj.m *= CMatrix2D::rotation(rrotate);
    }
  }
  else if (name == "flip" || name == "f") {
    if (! parseReal(adj.flip)) return false;

    if (compose) {
      double rflip = M_PI*adj.flip.getValue()/180.0;

      adj.m *= CMatrix2D::reflection(rflip);
    }
  }
  else if (name == "skew") {
    if (! parseReal(adj.skew_x)) return false;
    if (! parseReal(adj.skew_y)) return false;

    if (compose) {
      double rskewx = M_PI*adj.skew_x.getValue(0.0)/180.0;
      double rskewy = M_PI*adj.skew_y.getValue(0.0)/180.0;

      adj.m *= CMatrix2D::skew(rskewx, rskewy);
    }
  }
  else if (name == "hue" || name == "h") {
    if (! parseColorValue(adj.hue, &adj.thue)) return false;
  }
  else if (name == "saturation" || name == "sat") {
    if (! parseColorValue(adj.saturation, &adj.tsaturation)) return false;
  }
  else if (name == "brightness" || name == "b") {
    if (! parseColorValue(adj.brightness, &adj.tbrightness)) return false;
  }
  else if (name == "alpha" || name == "a") {
    if (! parseColorValue(adj.alpha, &adj.talpha)) return false;
  }
  else if (name == "|hue" || name == "|h") {
    if (! parseReal(adj.lhue)) return false;
  }
  else if (name == "|saturation" || name == "|sat") {
    if (! parseReal(adj.lsaturation)) return false;
  }
  else if (name == "|brightness" || name == "|b") {
    if (! parseReal(adj.lbrightness)) return false;
  }
  else if (name == "|alpha" || name == "|a") {
    if (! parseReal(adj.lalpha)) return false;
  }
  else {
    return false;
  }

  skipSpace();

  return true;
}

bool
CContextFree::
parsePath()
{
  // path <user_string> { <path_ops> }

  // <path_ops>: <path_ops> <path_op>

  // <path_op>: <user_pathop> { <points> }
  // <path_op>: <number> * <path_modification> <user_pathop> { <points> }
  // <path_op>: <number> * <path_modification> { <path_ops> }
  // <path_op>: <path_name> <path_modification>
  // <path_op>: <number> * <path_modification> <path_name> <path_modification>

  std::string id;

  parseName(id);

  Path *path = getPath(id);

  skipSpace();

  if (! parse_->isChar('{') && ! parse_->isChar('['))
    return false;

  char end_char = (parse_->isChar('{') ? '}' : ']');

  parse_->skipChar();

  skipSpace();

  ActionList *actionList = new ActionList(path, 1.0);

  PathAction *pathAction = new PathAction(path);

  actionList->addAction(pathAction);

  path->addActionList(actionList);

  while (! parse_->eof() && ! parse_->isChar(end_char)) {
    PathPart *part = parsePathPart();

    if (! part) return false;

    pathAction->addPart(part);
  }

  if (! parse_->eof() && parse_->isChar(end_char)) {
    parse_->skipChar();

    skipSpace();
  }

  return true;
}

CContextFree::PathPart *
CContextFree::
parsePathPart()
{
  if (parse_->isDigit()) {
    int n;

    if (! parse_->readInteger(&n)) return NULL;

    skipSpace();

    if (! parse_->isChar('*')) return NULL;

    parse_->skipChar();

    skipSpace();

    Adjustment nadj;

    if (! parseAdjustment(nadj)) return NULL;

    if (parse_->isChar('{') || parse_->isChar('[')) {
      char end_char = (parse_->isChar('{') ? '}' : ']');

      parse_->skipChar();

      skipSpace();

      LoopPathPartList *loopParts = new LoopPathPartList(n, nadj);

      while (! parse_->eof() && ! parse_->isChar(end_char)) {
        PathPart *part = parsePathPart();

        if (! part) return false;

        loopParts->addPart(part);
      }

      if (! parse_->eof() && parse_->isChar(end_char)) {
        parse_->skipChar();

        skipSpace();
      }

      return loopParts;
    }
    else {
      std::string name;

      if (! parseName(name)) return NULL;

      PathOp pathOp = lookupPathOp(name);

      if (pathOp == NO_PATH_OP) return NULL;

      PathPart *pathPart = parsePathOpPart(pathOp);
      if (pathPart == NULL) return NULL;

      LoopPathPart *loopPart = new LoopPathPart(n, nadj, pathPart);

      return loopPart;
    }
  }
  else {
    std::string name;

    if (! parseName(name)) return NULL;

    PathOp pathOp = lookupPathOp(name);

    if (pathOp == NO_PATH_OP) return NULL;

    PathPart *pathPart = parsePathOpPart(pathOp);

    return pathPart;
  }

  return NULL;
}

CContextFree::PathPart *
CContextFree::
parsePathOpPart(PathOp pathOp)
{
  PathPoints pathPoints;

  if (! parsePathPoints(pathOp, pathPoints)) return NULL;

  PathPart *pathPart = NULL;

  switch (pathOp) {
    case MOVE_TO_PATH_OP  : pathPart = new MoveToPathPart  (pathPoints); break;
    case RMOVE_TO_PATH_OP : return NULL;
    case LINE_TO_PATH_OP  : pathPart = new LineToPathPart  (pathPoints); break;
    case RLINE_TO_PATH_OP : return NULL;
    case ARC_TO_PATH_OP   : pathPart = new ArcToPathPart   (pathPoints); break;
    case RARC_TO_PATH_OP  : return NULL;
    case CURVE_TO_PATH_OP : pathPart = new CurveToPathPart (pathPoints); break;
    case RCURVE_TO_PATH_OP: return NULL;
    case CLOSE_PATH_OP    : pathPart = new ClosePathPart   (pathPoints); break;
    case STROKE_PATH_OP   : pathPart = new StrokePathPart  (pathPoints); break;
    case FILL_PATH_OP     : pathPart = new FillPathPart    (pathPoints); break;
    default               : assert(false); break;
  }

  return pathPart;
}

CContextFree::PathOp
CContextFree::
lookupPathOp(const string &name)
{
  if      (name == "MOVETO")    return MOVE_TO_PATH_OP;
  else if (name == "LINETO")    return LINE_TO_PATH_OP;
  else if (name == "ARCTO")     return ARC_TO_PATH_OP;
  else if (name == "CURVETO")   return CURVE_TO_PATH_OP;
  else if (name == "MOVEREL")   return RMOVE_TO_PATH_OP;
  else if (name == "LINEREL")   return RLINE_TO_PATH_OP;
  else if (name == "ARCREL")    return RARC_TO_PATH_OP;
  else if (name == "CURVEREL")  return RCURVE_TO_PATH_OP;
  else if (name == "CLOSEPOLY") return CLOSE_PATH_OP;
  else if (name == "STROKE")    return STROKE_PATH_OP;
  else if (name == "FILL")      return FILL_PATH_OP;
  else                          return NO_PATH_OP;
}

bool
CContextFree::
parsePathPoints(PathOp pathOp, PathPoints &points)
{
  if (! parse_->isChar('{')) return false;

  parse_->skipChar();

  skipSpace();

  while (! parse_->eof() && ! parse_->isChar('}')) {
    std::string name;

    if (! parseName(name)) return false;

    if (! parsePathValue(pathOp, name, points))
      error("Invalid path parameter " + name);
  }

  if (! parse_->isChar('}')) return false;

  parse_->skipChar();

  skipSpace();

  points.adj.buildMatrix();

  return true;
}

bool
CContextFree::
parsePathValue(PathOp pathOp, const std::string &name, PathPoints &points)
{
  if (pathOp == STROKE_PATH_OP || pathOp == FILL_PATH_OP) {
    if (parseAdjustmentValue(name, points.adj, false))
      return true;
  }

  if      (name == "x")     { return parseReal(points.x    ); }
  else if (name == "y")     { return parseReal(points.y    ); }
  else if (name == "x1")    { return parseReal(points.x1   ); }
  else if (name == "y1")    { return parseReal(points.y1   ); }
  else if (name == "x2")    { return parseReal(points.x2   ); }
  else if (name == "y2")    { return parseReal(points.y2   ); }
  else if (name == "rx")    { return parseReal(points.rx   ); }
  else if (name == "ry")    { return parseReal(points.ry   ); }
  else if (name == "r")     { return parseReal(points.r    ); }
  else if (name == "width") { return parseReal(points.width); }
  else if (name == "p" || name == "param") {
    std::string p; if (! parseName(p)) return false; points.p = p;
  }
  else { return false; }

  return true;
}

bool
CContextFree::
parseBlock1(std::string &str)
{
  if (! parse_->isChar('{')) return false;

  parse_->skipChar();

  skipSpace();

  str += '{';

  while (! parse_->eof() && ! parse_->isChar('}')) {
    if (parse_->isChar('{')) {
      std::string str1;

      if (! parseBlock1(str1)) return false;

      str += str1;
    }
    else {
      char c;

      parse_->readChar(&c);

      str += c;
    }
  }

  if (! parse_->isChar('}'))
    return false;

  parse_->skipChar();

  skipSpace();

  str += '}';

  return true;
}

bool
CContextFree::
parseName(std::string &name)
{
  skipSpace();

  name = "";

  if (! parse_->isAlpha() && ! parse_->isChar('_') && ! parse_->isChar('|'))
    return false;

  char c;

  parse_->readChar(&c);

  name += c;

  while (! parse_->eol() && (parse_->isAlpha() || parse_->isDigit() || parse_->isChar('_'))) {
    char c;

    parse_->readChar(&c);

    name += c;
  }

  skipSpace();

  return true;
}

bool
CContextFree::
parseColorValue(COptValT<double> &r, bool *target)
{
  double r1;

  if (! parseRealValue(&r1))
    return false;

  r.setValue(r1);

  if (parse_->isChar('|')) {
    parse_->skipChar();

    *target = true;
  }

  skipSpace();

  return true;
}

bool
CContextFree::
isReal()
{
  return (parse_->isDigit() || parse_->isChar('.') || parse_->isChar('-') ||
          parse_->isChar('+') || parse_->isChar('('));
}

bool
CContextFree::
parseReal(COptValT<double> &r)
{
  double r1;

  if (! parseReal(&r1))
    return false;

  r.setValue(r1);

  return true;
}

bool
CContextFree::
parseReal(double *r)
{
  if (! parseRealValue(r))
    return false;

  skipSpace();

  return true;
}

bool
CContextFree::
parseRealValue(double *r)
{
  if      (parse_->isDigit() || parse_->isChar('.') || parse_->isChar('-') || parse_->isChar('+')) {
    int sign = 1;

    if (parse_->isChar('-') || parse_->isChar('+')) {
      sign = (parse_->isChar('-') ? -1 : 1);

      parse_->skipChar();

      skipSpace();
    }

    if (! parse_->readReal(r)) {
      error("Invalid real");
      return false;
    }

    *r *= sign;
  }
  else if (parse_->isChar('(')) {
    parse_->skipChar();

    int depth = 1;

    std::string expr;

    while (! parse_->eof()) {
      if      (parse_->isChar('('))
        ++depth;
      else if (parse_->isChar(')')) {
        --depth;

        if (depth <= 0)
          break;
      }

      char c;

      parse_->readChar(&c);

      expr += c;
    }

    if (! parse_->eof() && parse_->isChar(')')) {
      parse_->skipChar();
    }

    CEval eval;

    eval.setForceReal(true);
    eval.setDegrees  (true);

    if (! eval.eval(expr, r)) {
      error("Invalid expression: " + expr);
      return false;
    }
  }
  else if (parse_->isAlpha()) {
    std::string expr;

    while (parse_->isAlpha() || parse_->isChar('_')) {
      char c;

      parse_->readChar(&c);

      expr += c;
    }

    if (! parse_->isChar('(')) {
      error("Missing (");
      return false;
    }

    expr += '(';

    parse_->skipChar();

    while (! parse_->eof() && ! parse_->isChar(')')) {
      char c;

      parse_->readChar(&c);

      expr += c;
    }

    if (! parse_->eof() && parse_->isChar(')')) {
      parse_->skipChar();
    }

    expr += ')';

    CEval eval;

    eval.setForceReal(true);
    eval.setDegrees  (true);

    if (! eval.eval(expr, r)) {
      error("Invalid expression: " + expr);
      return false;
    }
  }
  else {
    error("Invalid real char");
    return false;
  }

  return true;
}

void
CContextFree::
skipSpace()
{
  parse_->skipSpace();

  while (parse_->eol() && ! parse_->eof())
    parse_->skipSpace();

  while (skipComment()) {
    parse_->skipSpace();

    while (parse_->eol() && ! parse_->eof())
      parse_->skipSpace();
  }
}

bool
CContextFree::
skipComment()
{
  if (parse_->eof()) return false;

  if      (parse_->isChar('/') && parse_->isNextChar('*'))
    parseComment1();
  else if (parse_->isChar('/') && parse_->isNextChar('/'))
    parseComment2();
  else if (parse_->isChar('#'))
    parseComment3();
  else
    return false;

  return true;
}

void
CContextFree::
setStartShape(const std::string &name)
{
  if (start_shape_ == "")
    start_shape_ = name;
}

void
CContextFree::
fillBackground(const CHSVA &)
{
}

void
CContextFree::
fillSquare(double, double, double, double, const CMatrix2D &, const CHSVA &)
{
}

void
CContextFree::
fillCircle(double, double, double, const CMatrix2D &, const CHSVA &)
{
}

void
CContextFree::
fillTriangle(double, double, double, double, double, double, const CMatrix2D &, const CHSVA &)
{
}

void
CContextFree::
bufferRule(double z, Rule *rule, const State &state, const CBBox2D &bbox)
{
  zRuleStack_[int(100*z)].push_back(RuleState(rule, state, bbox.area()));
}

void
CContextFree::
pathInit()
{
}

void
CContextFree::
pathTerm()
{
}

void
CContextFree::
pathMoveTo(double, double)
{
}

void
CContextFree::
pathLineTo(double, double)
{
}

void
CContextFree::
pathCurveTo(double, double, double, double, double, double)
{
}

void
CContextFree::
pathClose()
{
}

void
CContextFree::
pathStroke(const CHSVA &, const CMatrix2D &, double)
{
}

void
CContextFree::
pathFill(const CHSVA &, const CMatrix2D &)
{
}

CContextFree::Rule *
CContextFree::
getRule(const std::string &id)
{
  if (rules_.empty()) {
    rules_["SQUARE"  ] = new SquareRule  (this, "SQUARE"  );
    rules_["CIRCLE"  ] = new CircleRule  (this, "CIRCLE"  );
    rules_["TRIANGLE"] = new TriangleRule(this, "TRIANGLE");
  }

  RuleMap::iterator p = rules_.find(id);

  if (p == rules_.end())
    p = rules_.insert(p, RuleMap::value_type(id, new Rule(this, id)));

  return (*p).second;
}

CContextFree::Path *
CContextFree::
getPath(const std::string &id)
{
  RuleMap::iterator p = rules_.find(id);

  if (p == rules_.end())
    p = rules_.insert(p, RuleMap::value_type(id, new Path(this, id)));

  return dynamic_cast<Path *>((*p).second);
}

void
CContextFree::
pushRule(Rule *rule, const State &state)
{
  if (rule->isBasic())
    rule->expand(state);
  else
    ruleStack_.push_back(RuleState(rule, state));
}

void
CContextFree::
dumpRuleStack()
{
  uint n = ruleStack_.size();

  for (uint i = 0; i < n; ++i) {
    if (i) cerr << " ";

    cerr << ruleStack_[i].getRule()->getName();
  }

  cerr << endl;
}

bool
CContextFree::
checkSizeLimit(const State &state)
{
  double sx, sy;

  state.m.getSize(&sx, &sy);

  double s = std::max(sx, sy);

  double ps = s/pixelSize_;

  return (ps < min_size_);
}

bool
CContextFree::
checkMaxShapes() const
{
  if (max_shapes_ == 0) return false;

  return (num_shapes_ >= max_shapes_);
}

double
CContextFree::
randIn(double min_val, double max_val)
{
  return (max_val - min_val)*((1.0*rand())/RAND_MAX) + min_val;
}

void
CContextFree::
error(const std::string &msg) const
{
  std::cerr << msg << std::endl;
  std::cerr << parse_->getBefore() + "[33m^[0m" + parse_->getAt() << std::endl;
}

//-------------

CContextFree::State
CContextFree::
adjustState(const State &state, const Adjustment &adj)
{
  State state1 = state;

  if (adj.z.isValid())
    state1.z += adj.z.getValue();

  if (adj.sz.isValid())
    state1.sz *= adj.sz.getValue();

  state1.m *= adj.m;

  if (adj.hue       .isValid() || adj.saturation.isValid() ||
      adj.brightness.isValid() || adj.alpha     .isValid()) {
    double h = adjustHueValue  (state1. color.getHue       (), adj. hue       .getValue(0.0),
                                state1.lcolor.getHue       (), adj.thue);
    double s = adjustColorValue(state1. color.getSaturation(), adj. saturation.getValue(0.0),
                                state1.lcolor.getSaturation(), adj.tsaturation);
    double b = adjustColorValue(state1. color.getValue     (), adj. brightness.getValue(0.0),
                                state1.lcolor.getValue     (), adj.tbrightness);
    double a = adjustColorValue(state1. color.getAlpha     (), adj. alpha     .getValue(0.0),
                                state1.lcolor.getAlpha     (), adj.talpha);

    state1.color = CHSVA(h, s, b, a);
  }

  if (adj.lhue       .isValid() || adj.lsaturation.isValid() ||
      adj.lbrightness.isValid() || adj.lalpha     .isValid()) {
    double h = adjustHueValue  (state1.lcolor.getHue       (), adj.lhue       .getValue(0.0));
    double s = adjustColorValue(state1.lcolor.getSaturation(), adj.lsaturation.getValue(0.0));
    double b = adjustColorValue(state1.lcolor.getValue     (), adj.lbrightness.getValue(0.0));
    double a = adjustColorValue(state1.lcolor.getAlpha     (), adj.lalpha     .getValue(0.0));

    state1.lcolor = CHSVA(h, s, b, a);
  }

  return state1;
}

double
CContextFree::
adjustHueValue(double base, double delta, double target, bool useTarget)
{
  double h;

  if (useTarget) {
    if (delta < 0) {
      if (target > base) target -= 360;

      h = base + (base - target)*delta;
    }
    else {
      if (target < base) target += 360;

      h = base + (target - base)*delta;
    }
  }
  else {
    h = base + delta;
  }

  while (h <    0.0) h += 360.0;
  while (h >= 360.0) h -= 360.0;

  return h;
}

double
CContextFree::
adjustColorValue(double base, double delta, double target, bool useTarget)
{
  double v = 0.0;

  if (useTarget) {
    if (delta > 0 && fabs(base - target) < 1E-5)
      v = base;
    else {
      double edge = (base < target ? 0 : 1);

      if (delta < 0)
        v = base + (  base - edge)*delta;
      else
        v = base + (target - base)*delta;
    }
  }
  else {
    if (delta < 0)
      v = base + base*delta;
    else
      v = base + (1 - base)*delta;
  }

  return std::min(std::max(v, 0.0), 1.0);
}

//--------------

void
CContextFree::Adjustment::
buildMatrix()
{
  m.setIdentity();

  CMatrix2D tr = CMatrix2D::translation(x.getValue(0.0), y.getValue(0.0));

  double rrotate = M_PI*rotate.getValue(0.0)/180.0;

  CMatrix2D rot = CMatrix2D::rotation(rrotate);

  CMatrix2D sc = CMatrix2D::scale(sx.getValue(1.0), sy.getValue(1.0));

  double rskewx = M_PI*skew_x.getValue(0.0)/180.0;
  double rskewy = M_PI*skew_y.getValue(0.0)/180.0;

  CMatrix2D sk = CMatrix2D::skew(rskewx, rskewy);

  CMatrix2D ref;

  if (flip.isValid()) {
    double rflip = M_PI*flip.getValue()/180.0;

    ref = CMatrix2D::reflection(rflip);
  }
  else
    ref.setIdentity();

  m = tr*rot*sc*sk*ref;
}

//--------------

CContextFreeParse::
CContextFreeParse(CContextFree *c, const string &filename) :
 c_(c), file_(NULL)
{
  file_ = new CFile(filename);
}

CContextFreeParse::
~CContextFreeParse()
{
  delete file_;
}

bool
CContextFreeParse::
fillBuffer()
{
  string line;

  while (true) {
    if (! file_->readLine(line))
      return false;

    uint len = line.size();

    uint i = 0;

    while (i < len && isspace(line[i]))
      ++i;

    if (i < len)
      break;
  }

  uint len = line.size();

  while (len > 0 && line[len - 1] == '\\') {
    line = line.substr(0, len - 1);

    string line1;

    if (file_->readLine(line1)) {
      uint len1 = line1.size();

      uint i = 0;

      while (i < len1 && isspace(line1[i]))
        ++i;

      line += " " + line1.substr(0);
    }

    len = line.size();
  }

  //if (getPos() == int(getLen())) setString(line);

  if (getPos() > 0)
    addString("\n" + line);
  else
    addString(line);

  return true;
}

bool
CContextFreeParse::
eol() const
{
  return CStrParse::eof();
}

bool
CContextFreeParse::
eof() const
{
  if (! CStrParse::eof())
    return false;

  CContextFreeParse *th = const_cast<CContextFreeParse *>(this);

  th->fillBuffer();

  if (! CStrParse::eof())
    return false;

  return file_->eof();
}

//-------------

CContextFree::Rule::
Rule(CContextFree *c, const std::string &id) :
 c_          (c),
 id_         (id),
 totalWeight_(0.0),
 actionLists_()
{
}

void
CContextFree::Rule::
addActionList(ActionList *actionList)
{
  actionLists_.push_back(actionList);

  totalWeight_ += actionList->getWeight();
}

void
CContextFree::Rule::
expand(const State &state)
{
  if (c_->checkMaxShapes()) return;

  ActionList *actionList = getActionList();

  if (actionList)
    actionList->expand(c_, state);
}

void
CContextFree::Rule::
exec(const State &state)
{
  ActionList *actionList = getActionList();

  if (actionList)
    actionList->exec(c_, state);
}

CContextFree::ActionList *
CContextFree::Rule::
getActionList()
{
  ActionList *actionList = NULL;

  uint num_action_lists = actionLists_.size();

  if      (num_action_lists == 1) {
    actionList = actionLists_[0];
  }
  else if (num_action_lists > 1) {
    double r = CContextFree::randIn(0.0, totalWeight_);

    double t1 = 0.0;
    double t2 = 0.0;

    for (uint i = 0; i < num_action_lists; ++ i) {
      actionList = actionLists_[i];

      t1 = t2;
      t2 = t1 + actionList->getWeight();

      if (r < t1 || r > t2) continue;

      break;
    }
  }

  return actionList;
}

//-------------

CContextFree::ActionList::
ActionList(Rule *rule, double weight) :
 rule_(rule), weight_(weight)
{
}

CContextFree::ActionList::
~ActionList()
{
  uint num = actions_.size();

  for (uint i = 0; i < num; ++i)
    delete actions_[i];
}

void
CContextFree::ActionList::
expand(CContextFree *c, const State &state)
{
  uint num_actions = actions_.size();

  for (uint i = 0; i < num_actions; ++i) {
    Action *action = actions_[i];

    action->expand(c, state);
  }
}

void
CContextFree::ActionList::
exec(CContextFree *c, const State &state)
{
  uint num_actions = actions_.size();

  for (uint i = 0; i < num_actions; ++i) {
    Action *action = actions_[i];

    action->exec(c, state);
  }
}

//-------------

void
CContextFree::SimpleAction::
expand(CContextFree *c, const State &state)
{
  if (! rule_) rule_ = c->getRule(getName());

  State state1 = adjustState(state, getAdjustment());

  if (c->checkSizeLimit(state1)) return;

  c->pushRule(rule_, state1);
}

void
CContextFree::SimpleAction::
exec(CContextFree *c, const State &state)
{
  if (! rule_) rule_ = c->getRule(getName());

  State state1 = adjustState(state, getAdjustment());

  if (c->checkSizeLimit(state1)) return;

  rule_->exec(state1);
}

//-------------

CContextFree::LoopAction::
LoopAction(int n, const Adjustment &nadj, const std::string &name, const Adjustment &adj) :
 Action(), n_(n), nadj_(nadj), name_(name), adj_(adj), rule_(NULL)
{
}

CContextFree::LoopAction::
~LoopAction()
{
}

void
CContextFree::LoopAction::
expand(CContextFree *c, const State &state)
{
  State state1 = state;

  if (! rule_) rule_ = c->getRule(getName());

  for (int i = 0; i < getLoopNum(); ++i) {
    State state2 = adjustState(state1, getAdjustment());

    if (c->checkSizeLimit(state2)) return;

    c->pushRule(rule_, state2);

    state1 = adjustState(state1, getLoopAdjustment());
  }
}

void
CContextFree::LoopAction::
exec(CContextFree *c, const State &state)
{
  State state1 = state;

  if (! rule_) rule_ = c->getRule(getName());

  for (int i = 0; i < getLoopNum(); ++i) {
    State state2 = adjustState(state1, getAdjustment());

    if (c->checkSizeLimit(state2)) return;

    rule_->exec(state2);

    state1 = adjustState(state1, getLoopAdjustment());
  }
}

//-------------

CContextFree::ComplexLoopAction::
ComplexLoopAction(int n, const Adjustment &nadj, Action *action) :
 Action(), n_(n), nadj_(nadj), action_(action)
{
}

CContextFree::ComplexLoopAction::
~ComplexLoopAction()
{
  delete action_;
}

void
CContextFree::ComplexLoopAction::
expand(CContextFree *c, const State &state)
{
  State state1 = state;

  for (int i = 0; i < getLoopNum(); ++i) {
    if (c->checkSizeLimit(state1)) return;

    action_->expand(c, state1);

    state1 = adjustState(state1, getLoopAdjustment());
  }
}

void
CContextFree::ComplexLoopAction::
exec(CContextFree *c, const State &state)
{
  State state1 = state;

  for (int i = 0; i < getLoopNum(); ++i) {
    if (c->checkSizeLimit(state1)) return;

    action_->exec(c, state1);

    state1 = adjustState(state1, getLoopAdjustment());
  }
}

//-------------

CContextFree::PathAction::
PathAction(Path *path) :
 Action(), path_(path)
{
}

CContextFree::PathAction::
~PathAction()
{
}

void
CContextFree::PathAction::
addPart(PathPart *part)
{
  parts_.push_back(part);
}

void
CContextFree::PathAction::
expand(CContextFree *c, const State &state)
{
  CContextFreePath *path = c->getPath();

  path->clear();

  uint num_parts = parts_.size();

  for (uint i = 0; i < num_parts; ++i)
    parts_[i]->expand(c, state);

  if (! path->getFilled() && ! path->getStroked()) {
    State state1;

    CBBox2D bbox;

    path->fillBBox(c, state1, bbox);

    c->updateBBox(bbox);
  }
}

void
CContextFree::PathAction::
exec(CContextFree *c, const State &state)
{
  CContextFreePath *path = c->getPath();

  path->clear();

  uint num_parts = parts_.size();

  for (uint i = 0; i < num_parts; ++i)
    parts_[i]->exec(c, state);

  if (! path->getFilled() && ! path->getStroked()) {
    State state1;

    path->fill(c, state1);
  }
}

//-------------

CContextFree::SquareRule::
SquareRule(CContextFree *c, const std::string &id) :
 Rule(c, id)
{
}

bool
CContextFree::SquareRule::
isBasic() const
{
  if (actionLists_.empty())
    return true;
  else
    return false;
}

void
CContextFree::SquareRule::
expand(const State &state)
{
  if (actionLists_.empty()) {
    double x1, y1, x2, y2, x3, y3, x4, y4;

    state.m.multiplyPoint(-0.5, -0.5, &x1, &y1);
    state.m.multiplyPoint( 0.5, -0.5, &x2, &y2);
    state.m.multiplyPoint( 0.5,  0.5, &x3, &y3);
    state.m.multiplyPoint(-0.5,  0.5, &x4, &y4);

    CBBox2D bbox;

    bbox.add(x1, y1);
    bbox.add(x2, y2);
    bbox.add(x3, y3);
    bbox.add(x4, y4);

    c_->updateBBox(bbox);

    //-----

    c_->bufferRule(state.z, this, state, bbox);

    c_->incNumShapes();
  }
  else
    Rule::expand(state);
}

void
CContextFree::SquareRule::
exec(const State &state)
{
  if (actionLists_.empty()) {
    CMatrix2D m = c_->getAdjustMatrix()*state.m;

    c_->fillSquare(-0.5, -0.5, 0.5, 0.5, m, state.color);
  }
  else
    Rule::exec(state);
}

//-------------

CContextFree::CircleRule::
CircleRule(CContextFree *c, const std::string &id) :
 Rule(c, id)
{
}

bool
CContextFree::CircleRule::
isBasic() const
{
  if (actionLists_.empty())
    return true;
  else
    return false;
}

void
CContextFree::CircleRule::
expand(const State &state)
{
  if (actionLists_.empty()) {
    // 4 bezier cures to represent a circle
    static const double t = 2*(sqrt(2.0) - 1)/3;

    static const double x11 =  0.5, y11 =  0.0;
    static const double x21 =  0.5, y21 =  t  , x31 =  t  , y31 =  0.5, x41 =  0.0, y41 =  0.5;
    static const double x22 = -t  , y22 =  0.5, x32 = -0.5, y32 =  t  , x42 = -0.5, y42 =  0.0;
    static const double x23 = -0.5, y23 = -t  , x33 = -t  , y33 = -0.5, x43 =  0.0, y43 = -0.5;
    static const double x24 =  t  , y24 = -0.5, x34 =  0.5, y34 = -t  , x44 =  0.5, y44 =  0.0;

    double tx[13], ty[13];

    state.m.multiplyPoint(x11, y11, &tx[ 0], &ty[ 0]);
    state.m.multiplyPoint(x21, y21, &tx[ 1], &ty[ 1]);
    state.m.multiplyPoint(x31, y31, &tx[ 2], &ty[ 2]);
    state.m.multiplyPoint(x41, y41, &tx[ 3], &ty[ 3]);
    state.m.multiplyPoint(x22, y22, &tx[ 4], &ty[ 4]);
    state.m.multiplyPoint(x32, y32, &tx[ 5], &ty[ 5]);
    state.m.multiplyPoint(x42, y42, &tx[ 6], &ty[ 6]);
    state.m.multiplyPoint(x23, y23, &tx[ 7], &ty[ 7]);
    state.m.multiplyPoint(x33, y33, &tx[ 8], &ty[ 8]);
    state.m.multiplyPoint(x43, y43, &tx[ 9], &ty[ 9]);
    state.m.multiplyPoint(x24, y24, &tx[10], &ty[10]);
    state.m.multiplyPoint(x34, y34, &tx[11], &ty[11]);
    state.m.multiplyPoint(x44, y44, &tx[12], &ty[12]);

    CBBox2D bbox;

    for (int i = 0; i < 13; ++i)
      bbox.add(tx[i], ty[i]);

    c_->updateBBox(bbox);

    //-----

    c_->bufferRule(state.z, this, state, bbox);

    c_->incNumShapes();
  }
  else
    Rule::expand(state);
}

void
CContextFree::CircleRule::
exec(const State &state)
{
  if (actionLists_.empty()) {
    CMatrix2D m = c_->getAdjustMatrix()*state.m;

    c_->fillCircle(0.0, 0.0, 0.5, m, state.color);
  }
  else
    Rule::exec(state);
}

//-------------

CContextFree::TriangleRule::
TriangleRule(CContextFree *c, const std::string &id) :
 Rule(c, id)
{
}

bool
CContextFree::TriangleRule::
isBasic() const
{
  if (actionLists_.empty())
    return true;
  else
    return false;
}

void
CContextFree::TriangleRule::
expand(const State &state)
{
  if (actionLists_.empty()) {
    static const double h1 = 0.5/sqrt(3.0);
    static const double h2 = 1.0/sqrt(3.0);

    double x1, y1, x2, y2, x3, y3;

    state.m.multiplyPoint( 0.0,  h2, &x1, &y1);
    state.m.multiplyPoint(-0.5, -h1, &x2, &y2);
    state.m.multiplyPoint( 0.5, -h1, &x3, &y3);

    CBBox2D bbox;

    bbox.add(x1, y1);
    bbox.add(x2, y2);
    bbox.add(x3, y3);

    c_->updateBBox(bbox);

    //-----

    c_->bufferRule(state.z, this, state, bbox);

    c_->incNumShapes();
  }
  else
    Rule::expand(state);
}

void
CContextFree::TriangleRule::
exec(const State &state)
{
  if (actionLists_.empty()) {
    static const double h1 = 0.5/sqrt(3.0);
    static const double h2 = 1.0/sqrt(3.0);

    CMatrix2D m = c_->getAdjustMatrix()*state.m;

    c_->fillTriangle(0.0, h2, -0.5, -h1, 0.5, -h1, m, state.color);
  }
  else
    Rule::exec(state);
}

//-------------

void
CContextFree::RuleState::
expand()
{
  CBBox2D bbox;

  rule_->expand(state_);

  if (bbox.isSet())
    area_ = bbox.area();
}

double
CContextFree::RuleState::
getArea() const
{
  return area_;
}

//-------------

CContextFree::Path::
Path(CContextFree *c, const std::string &name) :
 Rule(c, name)
{
}

void
CContextFree::Path::
expand(const State &state)
{
  CContextFreePath *path = c_->getPath();

  path->resetBBox();

  Rule::expand(state);

  c_->bufferRule(state.z, this, state, path->getBBox());

  c_->incNumShapes();
}

void
CContextFree::Path::
exec(const State &state)
{
  Rule::exec(state);
}

//-------------

void
CContextFree::MoveToPathPart::
expand(CContextFree *c, const State &)
{
  CContextFreePath *path = c->getPath();

  if (path->getStroked() || path->getFilled())
    path->clearParts();

  path->addPart(new CContextFreePathMoveTo(x_, y_));

  path->setCurrentPoint(x_, y_);

  path->setClosed(false);
}

void
CContextFree::MoveToPathPart::
exec(CContextFree *c, const State &state)
{
  expand(c, state);
}

//-------------

void
CContextFree::LineToPathPart::
expand(CContextFree *c, const State &)
{
  CContextFreePath *path = c->getPath();

  if (path->getStroked() || path->getFilled())
    path->clearParts();

  if (! path->getCurrentPointSet()) {
    double x1, y1; path->getCurrentPoint(&x1, &y1);

    path->addPart(new CContextFreePathMoveTo(x1, y1));
  }

  path->addPart(new CContextFreePathLineTo(x_, y_));

  path->setCurrentPoint(x_, y_);
}

void
CContextFree::LineToPathPart::
exec(CContextFree *c, const State &state)
{
  expand(c, state);
}

//-------------

void
CContextFree::ArcToPathPart::
expand(CContextFree *c, const State &)
{
  CContextFreePath *path = c->getPath();

  if (path->getStroked() || path->getFilled())
    path->clearParts();

  if (! path->getCurrentPointSet()) {
    double x1, y1; path->getCurrentPoint(&x1, &y1);

    path->addPart(new CContextFreePathMoveTo(x1, y1));
  }

  double x1, y1;

  path->getCurrentPoint(&x1, &y1);

  int    fs = fs_;
  double rx = rx_, ry = ry_;

  if (rx < 0 || ry < 0) { fs = 1 - fs; rx = fabs(rx); ry = fabs(ry); }

  double cx, cy, theta, delta;

  if (! CMathGeom2D::ConvertFromSVGArc(x1, y1, x_, y_, a_, rx, ry, fa_, fs,
                                       &cx, &cy, &theta, &delta)) return;

  double a1 = M_PI*theta/180.0;
  double a2 = M_PI*(theta + delta)/180.0;

  std::vector<C3Bezier2D> beziers;

  CArcToBezier::ArcToBeziers(cx, cy, rx, ry, a1, a2, beziers);

  uint num_beziers = beziers.size();

  for (uint i = 0; i < num_beziers; ++i) {
    const C3Bezier2D &bezier = beziers[i];

    double x2, y2, x3, y3, x4, y4;

    bezier.getControlPoint1(&x2, &y2);
    bezier.getControlPoint2(&x3, &y3);

    if (i < num_beziers - 1)
      bezier.getLastPoint(&x4, &y4);
    else {
      x4 = x_; y4 = y_;
    }

    path->addPart(new CContextFreePathCurve3To(x2, y2, x3, y3, x4, y4));
  }

  path->setCurrentPoint(x_, y_);
}

void
CContextFree::ArcToPathPart::
exec(CContextFree *c, const State &state)
{
  expand(c, state);
}

//-------------

void
CContextFree::CurveToPathPart::
expand(CContextFree *c, const State &)
{
  CContextFreePath *path = c->getPath();

  if (path->getStroked() || path->getFilled())
    path->clearParts();

  if (! path->getCurrentPointSet()) {
    double x1, y1; path->getCurrentPoint(&x1, &y1);

    path->addPart(new CContextFreePathMoveTo(x1, y1));
  }

  path->addPart(new CContextFreePathCurve3To(x_, y_, x1_, y1_, x2_, y2_));

  path->setCurrentPoint(x2_, y2_);
}

void
CContextFree::CurveToPathPart::
exec(CContextFree *c, const State &state)
{
  expand(c, state);
}

//-------------

void
CContextFree::ClosePathPart::
expand(CContextFree *c, const State &)
{
  CContextFreePath *path = c->getPath();

  if (! path->getCurrentPointSet()) return;

  path->addPart(new CContextFreePathClose());

  path->setClosed(true);
}

void
CContextFree::ClosePathPart::
exec(CContextFree *c, const State &state)
{
  expand(c, state);
}

//-------------

void
CContextFree::StrokePathPart::
expand(CContextFree *c, const State &state)
{
  CContextFreePath *path = c->getPath();

  State state1 = adjustState(state, adj_);

  CBBox2D bbox;

  path->strokeBBox(c, state1, w_.getValue(0.1), bbox);

  c->updateBBox(bbox);

  path->updateBBox(bbox);
}

void
CContextFree::StrokePathPart::
exec(CContextFree *c, const State &state)
{
  CContextFreePath *path = c->getPath();

  State state1 = adjustState(state, adj_);

  path->stroke(c, state1, w_.getValue(0.1));
}

//-------------

void
CContextFree::FillPathPart::
expand(CContextFree *c, const State &state)
{
  CContextFreePath *path = c->getPath();

  State state1 = adjustState(state, adj_);

  CBBox2D bbox;

  path->fillBBox(c, state1, bbox);

  c->updateBBox(bbox);

  path->updateBBox(bbox);
}

void
CContextFree::FillPathPart::
exec(CContextFree *c, const State &state)
{
  CContextFreePath *path = c->getPath();

  State state1 = adjustState(state, adj_);

  path->fill(c, state1);
}

//-------------

CContextFree::LoopPathPart::
LoopPathPart(int n, const Adjustment &adj, PathPart *part) :
 PathPart(LOOP_PATH_OP), n_(n), adj_(adj), part_(part)
{
}

CContextFree::LoopPathPart::
~LoopPathPart()
{
  delete part_;
}

void
CContextFree::LoopPathPart::
expand(CContextFree *c, const State &state)
{
  State state1 = state;

  for (int i = 0; i < n_; ++i) {
    part_->expand(c, state1);

    state1 = adjustState(state1, adj_);
  }
}

void
CContextFree::LoopPathPart::
exec(CContextFree *c, const State &state)
{
  State state1 = state;

  for (int i = 0; i < n_; ++i) {
    part_->exec(c, state1);

    state1 = adjustState(state1, adj_);
  }
}

//-------------

CContextFree::LoopPathPartList::
LoopPathPartList(int n, const Adjustment &adj) :
 PathPart(LOOP_PATH_LIST_OP), n_(n), adj_(adj)
{
}

CContextFree::LoopPathPartList::
~LoopPathPartList()
{
  uint num = parts_.size();

  for (uint i = 0; i < num; ++i)
    delete parts_[i];
}

void
CContextFree::LoopPathPartList::
expand(CContextFree *c, const State &state)
{
  State state1 = state;

  for (int i = 0; i < n_; ++i) {
    uint num_parts = parts_.size();

    for (uint j = 0; j < num_parts; ++j)
      parts_[j]->expand(c, state1);

    state1 = adjustState(state1, adj_);
  }
}

void
CContextFree::LoopPathPartList::
exec(CContextFree *c, const State &state)
{
  State state1 = state;

  for (int i = 0; i < n_; ++i) {
    uint num_parts = parts_.size();

    for (uint j = 0; j < num_parts; ++j)
      parts_[j]->exec(c, state1);

    state1 = adjustState(state1, adj_);
  }
}

//-------------

CContextFreePath::
CContextFreePath()
{
  clear();
}

CContextFreePath::
~CContextFreePath()
{
  clear();
}

void
CContextFreePath::
clear()
{
  clearParts();

  reset();
}

void
CContextFreePath::
clearParts()
{
  if (! parts_.empty()) {
    uint num_parts = parts_.size();

    for (uint i = 0; i < num_parts; ++i)
      delete parts_[i];

    parts_.clear();
  }

  stroked_ = false;
  filled_  = false;

  currentSet_ = false;
}

void
CContextFreePath::
reset()
{
  closed_  = false;
  stroked_ = false;
  filled_  = false;

  currentX_   = 0.0;
  currentY_   = 0.0;
  currentSet_ = false;
}

void
CContextFreePath::
addPart(CContextFreePathPart *part)
{
  parts_.push_back(part);
}

void
CContextFreePath::
strokeBBox(CContextFree *c, const CContextFree::State &state, double w, CBBox2D &bbox)
{
  uint num_parts = parts_.size();

  for (uint i = 0; i < num_parts; ++i)
    parts_[i]->updateBBox(c, state, bbox);

  bbox.expand(-w/2, -w/2, w/2, w/2);

  stroked_ = true;
}

void
CContextFreePath::
stroke(CContextFree *c, const CContextFree::State &state, double w)
{
  c->pathInit();

  uint num_parts = parts_.size();

  for (uint i = 0; i < num_parts; ++i)
    parts_[i]->addToPath(c);

  CMatrix2D m = c->getAdjustMatrix()*state.m;

  c->pathStroke(state.color, m, w);

  c->pathTerm();

  stroked_ = true;
}

void
CContextFreePath::
fillBBox(CContextFree *c, const CContextFree::State &state, CBBox2D &bbox)
{
  uint num_parts = parts_.size();

  for (uint i = 0; i < num_parts; ++i)
    parts_[i]->updateBBox(c, state, bbox);

  filled_ = true;
}

void
CContextFreePath::
fill(CContextFree *c, const CContextFree::State &state)
{
  c->pathInit();

  uint num_parts = parts_.size();

  for (uint i = 0; i < num_parts; ++i)
    parts_[i]->addToPath(c);

  CMatrix2D m = c->getAdjustMatrix()*state.m;

  c->pathFill(state.color, m);

  c->pathTerm();

  filled_ = true;
}

void
CContextFreePath::
term(CContextFree *c)
{
  if (! filled_ && ! stroked_)
    fill(c, CContextFree::State());
}

void
CContextFreePath::
setCurrentPoint(double x, double y)
{
  currentX_ = x;
  currentY_ = y;

  currentSet_ = true;
}

void
CContextFreePath::
resetCurrentPoint()
{
  currentSet_ = false;
}

bool
CContextFreePath::
getCurrentPoint(double *x, double *y) const
{
  *x = currentX_;
  *y = currentY_;

  return currentSet_;
}

void
CContextFreePath::
updateBBox(const CBBox2D &bbox)
{
  bbox_.add(bbox);
}

//-----

CContextFreePathMoveTo::
CContextFreePathMoveTo(double x, double y) :
 CContextFreePathPart(), x_(x), y_(y)
{
}

void
CContextFreePathMoveTo::
updateBBox(CContextFree *, const CContextFree::State &state, CBBox2D &bbox)
{
  double x, y;

  state.m.multiplyPoint(x_, y_, &x, &y);

  bbox.add(x, y);
}

void
CContextFreePathMoveTo::
addToPath(CContextFree *c)
{
  c->pathMoveTo(x_, y_);
}

//-----

CContextFreePathLineTo::
CContextFreePathLineTo(double x, double y) :
 CContextFreePathPart(), x_(x), y_(y)
{
}

void
CContextFreePathLineTo::
updateBBox(CContextFree *, const CContextFree::State &state, CBBox2D &bbox)
{
  double x, y;

  state.m.multiplyPoint(x_, y_, &x, &y);

  bbox.add(x, y);
}

void
CContextFreePathLineTo::
addToPath(CContextFree *c)
{
  c->pathLineTo(x_, y_);
}

//-----

CContextFreePathCurve3To::
CContextFreePathCurve3To(double x2, double y2, double x3, double y3, double x4, double y4) :
 CContextFreePathPart(), x2_(x2), y2_(y2), x3_(x3), y3_(y3), x4_(x4), y4_(y4)
{
}

void
CContextFreePathCurve3To::
updateBBox(CContextFree *, const CContextFree::State &state, CBBox2D &bbox)
{
  double x2, y2, x3, y3, x4, y4;

  state.m.multiplyPoint(x2_, y2_, &x2, &y2);
  state.m.multiplyPoint(x3_, y3_, &x3, &y3);
  state.m.multiplyPoint(x4_, y4_, &x4, &y4);

  bbox.add(x2, y2);
  bbox.add(x3, y3);
  bbox.add(x4, y4);
}

void
CContextFreePathCurve3To::
addToPath(CContextFree *c)
{
  c->pathCurveTo(x2_, y2_, x3_, y3_, x4_, y4_);
}

//-----

CContextFreePathClose::
CContextFreePathClose() :
 CContextFreePathPart()
{
}

void
CContextFreePathClose::
updateBBox(CContextFree *, const CContextFree::State &, CBBox2D &)
{
}

void
CContextFreePathClose::
addToPath(CContextFree *c)
{
  c->pathClose();
}
