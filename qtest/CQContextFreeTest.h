#include <CContextFree.h>
#include <QWidget>
#include <QImage>

class QProgressBar;
class CQContextFreeTest;

class CQContextFree : public CContextFree {
 public:
  CQContextFree(CQContextFreeTest *c);

  void setMatrix(const CMatrix2D &m) { m_ = m; }

  void exec();

  bool tick();

  void fillBackground(const CHSVA &hsva);

  void fillSquare  (double x1, double y1, double x2, double y2, const CMatrix2D &m,
                    const CHSVA &color);
  void fillCircle  (double x, double y, double r, const CMatrix2D &m, const CHSVA &color);
  void fillTriangle(double x1, double y1, double x2, double y2, double x3, double y3,
                    const CMatrix2D &m, const CHSVA &color);

  void pathInit   ();
  void pathTerm   ();
  void pathMoveTo (double x, double y);
  void pathLineTo (double x, double y);
  void pathCurveTo(double x2, double y2, double x3, double y3, double x4, double y4);
  void pathClose  ();
  void pathStroke (const CHSVA &color, const CMatrix2D &m, double w);
  void pathFill   (const CHSVA &color, const CMatrix2D &m);

  static QColor getColor(const CHSVA &hsv);

  QTransform toQTransform(const CMatrix2D &m);

  void setQuit(bool quit) { quit_ = quit; }
  bool getQuit() const { return quit_; }

 private:
  CQContextFreeTest *c_;
  CMatrix2D          m_;
  QPainterPath      *path_;
  bool               quit_;
};

class CQContextFreeTest : public QWidget {
  Q_OBJECT

 public:
  CQContextFreeTest();

  void addFile(const std::string &filename);

  bool loadFile(int pos);

  void showNext();
  void showPrev();
  void showCurrent();

  void setMaxShapes(uint max_shapes);
  void setMinSize  (double min_size);
  void setBorder   (int border);
  void setAntiAlias(bool anti_alias);

  int getWidth () const { return w_; }
  int getHeight() const { return h_; }

  QImage &getImage() { return image_; }

  QPainter *getIPainter() const { return ipainter_; }

  void tick();

 private:
  void resizeEvent(QResizeEvent *);

  void paintEvent(QPaintEvent *e);

  void mousePressEvent  (QMouseEvent *e);
  void mouseReleaseEvent(QMouseEvent *e);

  void keyPressEvent(QKeyEvent *);

  void setViewTransform();

 private:
  typedef std::vector<std::string> StringArray;

  CQContextFree *c_;
  QProgressBar  *progress_;
  StringArray    filenames_;
  int            pos_;
  bool           first_;
  double         xmin_, ymin_, xmax_, ymax_;
  int            w_, h_;
  QImage         image_;
  QPainter      *ipainter_;
  int            border_;
  bool           anti_alias_;
  double         press_x_, press_y_;
  bool           pressed_;
};
