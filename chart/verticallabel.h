#ifndef VERTICALLABEL_H
#define VERTICALLABEL_H

#include <QLabel>

///
/// \brief The VerticalLabel class draws a label vertically by rotating a
/// horizontal label
///
class VerticalLabel : public QLabel {
public:
    VerticalLabel(QWidget* parent = nullptr);
    VerticalLabel(QString const& text, QWidget* parent = nullptr);
    ~VerticalLabel() override;

    void  paintEvent(QPaintEvent*) override;
    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;
};

#endif // VERTICALLABEL_H
