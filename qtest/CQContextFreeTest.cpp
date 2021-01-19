#include <CQContextFreeTest.h>
#include <CRGBUtil.h>

#include <QApplication>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QProgressBar>

using std::cerr;
using std::endl;

int
main(int argc, char **argv)
{
  QApplication app(argc, argv);

  CQContextFreeTest *c = new CQContextFreeTest;

  int    width      = 500;
  int    height     = 500;
  double min_size   = 0.3;
  int    max_shapes = 500000;
  int    border     = 0;
  bool   anti_alias = true;

  std::vector<std::string> filenames;

  for (int i = 1; i < argc; ++i) {
    if (argv[i][0] == '-') {
      if      (strcmp(argv[i], "-antialias") == 0 || strcmp(argv[i], "-anti_alias") == 0)
        anti_alias = true;
      else if (strcmp(argv[i], "-noantialias") == 0 || strcmp(argv[i], "-no_anti_alias") == 0)
        anti_alias = false;
      else if (strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "-width") == 0) {
        ++i; if (i < argc) width = atoi(argv[i]);
      }
      else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "-height") == 0) {
        ++i; if (i < argc) height = atoi(argv[i]);
      }
      else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "-size") == 0) {
        ++i; if (i < argc) { width = atoi(argv[i]); height = width; }
      }
      else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "-max_shapes") == 0) {
        ++i; if (i < argc) max_shapes = atoi(argv[i]);
      }
      else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "-border") == 0) {
        ++i; if (i < argc) border = atoi(argv[i]);
      }
      else if (strcmp(argv[i], "-x") == 0 || strcmp(argv[i], "-min_size") == 0) {
        ++i; if (i < argc) min_size = atof(argv[i]);
      }
      else {
        cerr << "Invalid option " << argv[i] << endl;
      }
    }
    else
      filenames.push_back(argv[i]);
  }

  uint num_files = filenames.size();

  for (uint i = 0; i < num_files; ++i)
    c->addFile(filenames[i]);

  c->setMaxShapes(max_shapes);
  c->setMinSize  (min_size);
  c->setBorder   (border);
  c->setAntiAlias(anti_alias);

  c->resize(width, height);

  c->show();

  c->showNext();

  app.exec();

  return 0;
}

CQContextFreeTest::
CQContextFreeTest() :
 QWidget(), pos_(-1), first_(true), w_(100), h_(100), image_(), ipainter_(NULL),
 border_(0), anti_alias_(false), pressed_(false)
{
  xmin_ = -2.0, ymin_ = -2.0;
  xmax_ =  2.0, ymax_ =  2.0;

  c_ = new CQContextFree(this);

  progress_ = new QProgressBar(this);

  progress_->setAutoFillBackground(true);
}

void
CQContextFreeTest::
addFile(const std::string &filename)
{
  filenames_.push_back(filename);
}

void
CQContextFreeTest::
showNext()
{
  ++pos_;

  if (pos_ >= int(filenames_.size())) {
    pos_ = filenames_.size() - 1;
    return;
  }

  showCurrent();
}

void
CQContextFreeTest::
showPrev()
{
  --pos_;

  if (pos_ < 0) {
    pos_ = 0;
    return;
  }

  showCurrent();
}

void
CQContextFreeTest::
showCurrent()
{
  c_->setQuit(false);

  loadFile(pos_);
}

void
CQContextFreeTest::
setAntiAlias(bool anti_alias)
{
  anti_alias_ = anti_alias;
}

void
CQContextFreeTest::
setMaxShapes(uint max_shapes)
{
  c_->setMaxShapes(max_shapes);
}

void
CQContextFreeTest::
setBorder(int border)
{
  border_ = border;
}

void
CQContextFreeTest::
setMinSize(double min_size)
{
  c_->setMinSize(min_size);
}

bool
CQContextFreeTest::
loadFile(int pos)
{
  const std::string &filename = filenames_[pos];

  QString title =
    QString("%1 (%2 of %3)").arg(filename.c_str()).arg(pos_ + 1).arg(filenames_.size());

  setWindowTitle(title);

  c_->reset();

  if (! c_->parse(filename))
    return false;

  first_ = true;

  resizeEvent(NULL);

  update();

  return true;
}

void
CQContextFreeTest::
resizeEvent(QResizeEvent *)
{
  w_ = width ();
  h_ = height();

  delete ipainter_;

  image_ = QImage(w_, h_, QImage::Format_ARGB32);

  ipainter_ = new QPainter(&image_);

  if (anti_alias_)
    ipainter_->setRenderHints(QPainter::Antialiasing);

  //-----

  if (pos_ < 0) return;

  //-----

  if (first_) {
    c_->setPixelSize(std::min((xmax_ - xmin_)/w_, (ymax_ - ymin_)/h_));

    c_->setQuit(false);

    progress_->show();

    c_->expand();

    progress_->hide();

    if (! c_->getTile(&xmin_, &ymin_, &xmax_, &ymax_)) {
      const CBBox2D &bbox = c_->getBBox();

      double w = bbox.getWidth ();
      double h = bbox.getHeight();

      double b = 0.0;

      if (border_ > 0)
        b = std::min((border_*w)/100.0, (border_*h)/100.0);

      xmin_ = bbox.getXMin() - b;
      ymin_ = bbox.getYMin() - b;
      xmax_ = bbox.getXMax() + b;
      ymax_ = bbox.getYMax() + b;
    }

    c_->setPixelSize(std::min((xmax_ - xmin_)/w_, (ymax_ - ymin_)/h_));

    first_ = false;
  }

  if (! c_->getQuit()) {
    setViewTransform();

    c_->render();
  }
}

void
CQContextFreeTest::
paintEvent(QPaintEvent *)
{
  QPainter p(this);

  p.drawImage(0, 0, image_);
}

void
CQContextFreeTest::
mousePressEvent(QMouseEvent *e)
{
  pressed_ = true;

  press_x_ = (xmax_ - xmin_)*(1.0*e->pos().x()/(w_ - 1)) + xmin_;
  press_y_ = (ymin_ - ymax_)*(1.0*e->pos().y()/(h_ - 1)) + ymax_;

  cerr << press_x_ << " " << press_y_ << endl;
}

void
CQContextFreeTest::
mouseReleaseEvent(QMouseEvent *e)
{
  pressed_ = false;

  double x = (xmax_ - xmin_)*(1.0*e->pos().x()/(w_ - 1)) + xmin_;
  double y = (ymin_ - ymax_)*(1.0*e->pos().y()/(h_ - 1)) + ymax_;

  cerr << x << " " << y << endl;

  xmin_ = std::min(x, press_x_);
  ymin_ = std::min(y, press_y_);
  xmax_ = std::max(x, press_x_);
  ymax_ = std::max(y, press_y_);

  setViewTransform();

  c_->render();

  update();
}

void
CQContextFreeTest::
keyPressEvent(QKeyEvent *e)
{
  if      (e->key() == Qt::Key_N)
    showNext();
  else if (e->key() == Qt::Key_P)
    showPrev();
  else if (e->key() == Qt::Key_R)
    showCurrent();
  else if (e->key() == Qt::Key_Escape)
    c_->setQuit(true);
  else if (e->key() == Qt::Key_Q)
    exit(0);
}

void
CQContextFreeTest::
setViewTransform()
{
  double w = xmax_ - xmin_;
  double h = ymax_ - ymin_;

  double sx = (w_ - 1)/w;
  double sy = (h_ - 1)/h;

  double s = std::min(sx, sy);

  int dx = (w_ - s*w)/2;
  int dy = (h_ - s*h)/2;

  CMatrix2D m1 = CMatrix2D::translation(dx, dy);
  CMatrix2D m2 = CMatrix2D::scale(s, -s);
  CMatrix2D m3 = CMatrix2D::translation(-xmin_, -ymax_);

  CMatrix2D m = m1*m2*m3;

  c_->setMatrix(m);
}

void
CQContextFreeTest::
tick()
{
  QApplication::processEvents();

  progress_->setMaximum(c_->getMaxShapes());
  progress_->setValue  (c_->getNumShapes());

  progress_->update();
}

//------

CQContextFree::
CQContextFree(CQContextFreeTest *c) :
 c_(c), path_(NULL), quit_(false)
{
  m_.setIdentity();
}

bool
CQContextFree::
tick()
{
  c_->tick();

  return ! quit_;
}

void
CQContextFree::
fillBackground(const CHSVA &hsv)
{
  QColor c = getColor(hsv);

  c_->getImage().fill(c.rgba());
}

void
CQContextFree::
fillSquare(double x1, double y1, double x2, double y2, const CMatrix2D &m, const CHSVA &color)
{
  QPainterPath path;

  path.moveTo(x1, y1);
  path.lineTo(x2, y1);
  path.lineTo(x2, y2);
  path.lineTo(x1, y2);

  path.closeSubpath();

  QColor c = getColor(color);

  QPainter *ipainter = c_->getIPainter();

  QTransform t  = ipainter->transform();
  QTransform t1 = toQTransform(m_*m);

  ipainter->setTransform(t1*t);

  ipainter->fillPath(path, QBrush(c));

  ipainter->setTransform(t);
}

void
CQContextFree::
fillCircle(double x, double y, double r, const CMatrix2D &m, const CHSVA &color)
{
  QPainterPath path;

  path.addEllipse(QRectF(x - r, y - r, 2*r, 2*r));

  QColor c = getColor(color);

  QPainter *ipainter = c_->getIPainter();

  QTransform t  = ipainter->transform();
  QTransform t1 = toQTransform(m_*m);

  ipainter->setTransform(t1*t);

  ipainter->fillPath(path, QBrush(c));

  ipainter->setTransform(t);
}

void
CQContextFree::
fillTriangle(double x1, double y1, double x2, double y2, double x3, double y3,
             const CMatrix2D &m, const CHSVA &color)
{
  QPainterPath path;

  path.moveTo(x1, y1);
  path.lineTo(x2, y2);
  path.lineTo(x3, y3);

  path.closeSubpath();

  QColor c = getColor(color);

  QPainter *ipainter = c_->getIPainter();

  QTransform t  = ipainter->transform();
  QTransform t1 = toQTransform(m_*m);

  ipainter->setTransform(t1*t);

  ipainter->fillPath(path, QBrush(c));

  ipainter->setTransform(t);
}

void
CQContextFree::
pathInit()
{
  path_ = new QPainterPath;
}

void
CQContextFree::
pathTerm()
{
  delete path_;

  path_ = NULL;
}

void
CQContextFree::
pathMoveTo(double x, double y)
{
  path_->moveTo(QPointF(x, y));
}

void
CQContextFree::
pathLineTo(double x, double y)
{
  path_->lineTo(QPointF(x, y));
}

void
CQContextFree::
pathCurveTo(double x2, double y2, double x3, double y3, double x4, double y4)
{
  path_->cubicTo(QPointF(x2, y2), QPointF(x3, y3), QPointF(x4, y4));
}

void
CQContextFree::
pathClose()
{
  path_->closeSubpath();
}

void
CQContextFree::
pathStroke(const CHSVA &color, const CMatrix2D &m, double w)
{
  QPainter *ipainter = c_->getIPainter();

  QTransform t  = ipainter->transform();
  QTransform t1 = toQTransform(m_*m);

  QColor c = CQContextFree::getColor(color);

  QPen pen(c);

  pen.setWidthF   (w);
  pen.setJoinStyle(Qt::MiterJoin);
  pen.setCapStyle (Qt::FlatCap);

  ipainter->setTransform(t1*t);

  ipainter->strokePath(*path_, pen);

  ipainter->setTransform(t);
}

void
CQContextFree::
pathFill(const CHSVA &color, const CMatrix2D &m)
{
  QPainter *ipainter = c_->getIPainter();

  QTransform t  = ipainter->transform();
  QTransform t1 = toQTransform(m_*m);

  QColor c = CQContextFree::getColor(color);

  QBrush brush(c);

  ipainter->setTransform(t1*t);

  ipainter->fillPath(*path_, brush);

  ipainter->setTransform(t);
}

QColor
CQContextFree::
getColor(const CHSVA &hsv)
{
  auto rgba = CRGBUtil::HSVAtoRGBA(hsv);

  int r = rgba.getRedI  ();
  int g = rgba.getGreenI();
  int b = rgba.getBlueI ();
  int a = rgba.getAlphaI();

  QColor c(r, g, b, a);

  return c;
}

QTransform
CQContextFree::
toQTransform(const CMatrix2D &m)
{
  double a, b, c, d, tx, ty;

  m.getValues(&a, &b, &c, &d, &tx, &ty);

  return QTransform(a, c, b, d, tx, ty);
}
