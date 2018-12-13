#include "verticallabel.h"

#include <QDebug>
#include <QPainter>

VerticalLabel::VerticalLabel(QWidget* parent) : QLabel(parent) {}

VerticalLabel::VerticalLabel(QString const& text, QWidget* parent)
    : QLabel(text, parent) {}

VerticalLabel::~VerticalLabel() = default;

void VerticalLabel::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setPen(Qt::white);

    painter.translate(0, height());
    painter.rotate(270);

    auto ltext = QFontMetrics(font()).elidedText(
        text(), Qt::TextElideMode::ElideRight, height());

    painter.drawText(QRect(0, 0, height(), width()), Qt::AlignCenter, ltext);
}

QSize VerticalLabel::minimumSizeHint() const {
    QSize s = QLabel::minimumSizeHint();
    return QSize(s.height(), s.width());
}

QSize VerticalLabel::sizeHint() const {
    QSize s = QLabel::sizeHint();
    return QSize(s.height(), s.width());
}
