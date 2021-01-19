#include <CContextFree.h>
#include <CStrUtil.h>
#include <CRGBUtil.h>

class CContextFreeTest : public CContextFree {
 public:
  CContextFreeTest() :
   CContextFree() {
  }

  void process(const char *str);

  void beginRule(Rule *rule);
  void endRule  (Rule *rule);

  void fillSquare  (double x1, double y1, double x2, double y2, const CMatrix2D &m,
                    const CHSVA &hsv);
  void fillCircle  (double x, double y, double r, const CMatrix2D &m, const CHSVA &hsv);
  void fillTriangle(double x1, double y1, double x2, double y2, double x3, double y3,
                    const CMatrix2D &m, const CHSVA &hsv);

  void pathInit   ();
  void pathTerm   ();
  void pathMoveTo (double x, double y);
  void pathLineTo (double x, double y);
  void pathCurveTo(double x, double y, double x1, double y1, double x2, double y2);
  void pathClose  ();
  void pathStroke (const CHSVA &hsv, const CMatrix2D &m, double w);
  void pathFill   (const CHSVA &hsv, const CMatrix2D &m);

 private:
  int         id_ { 0 };
  std::string pathStr_;
  CMatrix2D   m_;
};

int
main(int argc, char **argv)
{
  CContextFreeTest *c = new CContextFreeTest;

  for (int i = 1; i < argc; ++i)
    c->process(argv[i]);

  //delete c;

  return 0;
}

void
CContextFreeTest::
process(const char *filename)
{
  int pw = 500;
  int ph = 500;

  parse(filename);

  expand();

  const CBBox2D &bbox = getBBox();

  double xmin, ymin, xmax, ymax;

  if (! getTile(&xmin, &ymin, &xmax, &ymax)) {
    xmin = bbox.getXMin();
    ymin = bbox.getYMin();
    xmax = bbox.getXMax();
    ymax = bbox.getYMax();
  }

  double w = xmax - xmin;
  double h = ymax - ymin;

  //double sx = (pw - 1)/w;
  //double sy = (ph - 1)/h;
  double sx = pw/w;
  double sy = ph/h;

  double s = std::min(sx, sy);

  int dx = (pw - s*w)/2;
  int dy = (ph - s*h)/2;

  CMatrix2D m1 = CMatrix2D::translation(dx, dy);
  CMatrix2D m2 = CMatrix2D::scale(s, -s);
  CMatrix2D m3 = CMatrix2D::translation(-xmin, -ymax);

  m_ = m1*m2*m3;

  std::cout << "<svg width=\"" << pw << "px\" height=\"" << ph << "px\"";

  //cout << "viewBox=\"" << xmin << " " << ymin << " " << xmax << " " << ymax << "\"";

  std::cout << ">\n";

  render();

  std::cout << "</svg>\n";
}

void
CContextFreeTest::
beginRule(Rule *rule)
{
  std::cout << "<g id=\"" << rule->getName() << id_ << "\">\n";

  ++id_;
}

void
CContextFreeTest::
endRule(Rule * /*rule*/)
{
  std::cout << "</g>\n";
}

void
CContextFreeTest::
fillSquare(double x1, double y1, double x2, double y2, const CMatrix2D &m, const CHSVA &hsv)
{
  auto rgba = CRGBUtil::HSVAtoRGBA(hsv);

  int r = rgba.getRedI  ();
  int g = rgba.getGreenI();
  int b = rgba.getBlueI ();
  int a = rgba.getAlphaI();

  std::cout << "<rect x=\"" << x1 << "\" y=\"" << y1 << "\"" <<
               " width=\"" << (x2 - x1) << "\" height=\"" << (y2 - y1) << "\"";

  std::cout << " stroke=\"none\"";

  std::cout << " fill=\"rgb(" << r << "," << g << "," << b << ")\"";

  if (a < 255) std::cout << " fill-opacity=\"" << a/255.0 << "\"";

  double v[6];

  CMatrix2D m1 = m_*m;

  m1.getValues(v, 6);

  std::cout << " transform=\"matrix(" << v[0] << " " << v[1] << " " << v[2] << " " <<
                                         v[3] << " " << v[4] << " " << v[5] << ")\"";

  std::cout << "/>\n";
}

void
CContextFreeTest::
fillCircle(double x, double y, double radius, const CMatrix2D &m, const CHSVA &hsv)
{
  auto rgba = CRGBUtil::HSVAtoRGBA(hsv);

  int r = rgba.getRedI  ();
  int g = rgba.getGreenI();
  int b = rgba.getBlueI ();
  int a = rgba.getAlphaI();

  std::cout << "<circle x=\"" << x << " y=\"" << y << " r=\"" << radius << "\"";

  std::cout << " stroke=\"none\"";

  std::cout << " fill=\"rgb(" << r << "," << g << "," << b << ")\"";

  if (a < 255) std::cout << " fill-opacity=\"" << a/255.0 << "\"";

  double v[6];

  CMatrix2D m1 = m_*m;

  m1.getValues(v, 6);

  std::cout << " transform=\"matrix(" << v[0] << " " << v[1] << " " << v[2] << " " <<
                                         v[3] << " " << v[4] << " " << v[5] << ")\"";

  std::cout << "/>\n";
}

void
CContextFreeTest::
fillTriangle(double x1, double y1, double x2, double y2, double x3, double y3,
             const CMatrix2D &m, const CHSVA &hsv)
{
  auto rgba = CRGBUtil::HSVAtoRGBA(hsv);

  int r = rgba.getRedI  ();
  int g = rgba.getGreenI();
  int b = rgba.getBlueI ();
  int a = rgba.getAlphaI();

  std::cout << "<polygon points=\"" << x1 << " " << y1 << " " << x2 << " " << y2 <<
               " " << x3 << " " << y3 << "\"";

  std::cout << " stroke=\"none\"";

  std::cout << " fill=\"rgb(" << r << "," << g << "," << b << ")\"";

  if (a < 255) std::cout << " fill-opacity=\"" << a/255.0 << "\"";

  double v[6];

  CMatrix2D m1 = m_*m;

  m1.getValues(v, 6);

  std::cout << " transform=\"matrix(" << v[0] << " " << v[1] << " " << v[2] << " " <<
                                         v[3] << " " << v[4] << " " << v[5] << ")\"";

  std::cout << "/>\n";
}

void
CContextFreeTest::
pathInit()
{
  pathStr_ = "<path d=\"";
}

void
CContextFreeTest::
pathTerm()
{
}

void
CContextFreeTest::
pathMoveTo(double x, double y)
{
  pathStr_ += "M " + CStrUtil::toString(x) + " " + CStrUtil::toString(y) + " ";
}

void
CContextFreeTest::
pathLineTo(double x, double y)
{
  pathStr_ += "L " + CStrUtil::toString(x) + " " + CStrUtil::toString(y) + " ";
}

void
CContextFreeTest::
pathCurveTo(double x, double y, double x1, double y1, double x2, double y2)
{
  pathStr_ += "C " + CStrUtil::toString(x ) + " " + CStrUtil::toString(y ) + " ";
  pathStr_ +=        CStrUtil::toString(x1) + " " + CStrUtil::toString(y1) + " ";
  pathStr_ +=        CStrUtil::toString(x2) + " " + CStrUtil::toString(y2) + " ";
}

void
CContextFreeTest::
pathClose()
{
  pathStr_ += "Z ";
}

void
CContextFreeTest::
pathStroke(const CHSVA &hsv, const CMatrix2D &m, double w)
{
  auto rgba = CRGBUtil::HSVAtoRGBA(hsv);

  int r = rgba.getRedI  ();
  int g = rgba.getGreenI();
  int b = rgba.getBlueI ();
  int a = rgba.getAlphaI();

  std::cout << "<g style=\"";

  std::cout << "fill: none; stroke: rgb(" << r << "," << g << "," << b << ");";

  std::cout << " stroke-width: " << w << ";";
  std::cout << " stroke-linecap: " << "miter" << ";";
  std::cout << " stroke-linejoin: " << "miter" << ";";
  std::cout << " stroke-miterlimit: " << "4" << ";";

  if (a < 255) std::cout << " stroke-opacity: " << a/255.0 << ";";

  std::cout << "\"";

  double v[6];

  CMatrix2D m1 = m_*m;

  m1.getValues(v, 6);

  std::cout << " transform=\"matrix(" << v[0] << " " << v[1] << " " << v[2] << " " <<
                                         v[3] << " " << v[4] << " " << v[5] << ")\"";

  std::cout << ">\n";

  std::cout << "  " << pathStr_ << "\"/>\n";

  std::cout << "</g>\n";
}

void
CContextFreeTest::
pathFill(const CHSVA &hsv, const CMatrix2D &m)
{
  auto rgba = CRGBUtil::HSVAtoRGBA(hsv);

  int r = rgba.getRedI  ();
  int g = rgba.getGreenI();
  int b = rgba.getBlueI ();
  int a = rgba.getAlphaI();

  std::cout << "<g style=\"";

  std::cout << "stroke: none; fill: rgb(" << r << "," << g << "," << b << ");";

  if (a < 255) std::cout << " fill-opacity: " << a/255.0 << "; ";

  std::cout << "\"";

  double v[6];

  CMatrix2D m1 = m_*m;

  m1.getValues(v, 6);

  std::cout << " transform=\"matrix(" << v[0] << " " << v[1] << " " << v[2] << " " <<
                                         v[3] << " " << v[4] << " " << v[5] << ")\"";

  std::cout << ">\n";

  std::cout << "  " << pathStr_ << "\"/>\n";

  std::cout << "</g>\n";
}
