#pragma once

#include <QColor>
#include <QHexView/model/qhexutils.h>
#include <QObject>

struct QHexCharFormat {
    QColor background{Qt::transparent};
    QColor foreground;
    QColor underline;
};

class QHexView;
class QPainter;

class QHexDelegate: public QObject {
    Q_OBJECT

public:
    explicit QHexDelegate(QObject* parent = nullptr);
    virtual ~QHexDelegate() = default;
    virtual QString addressHeader(const QHexView* hexview) const;
    virtual QString hexHeader(const QHexView* hexview) const;
    virtual QString asciiHeader(const QHexView* hexview) const;
    virtual void renderAddress(quint64 address, QHexCharFormat& cf,
                               const QHexView* hexview) const;
    virtual void renderHeader(QHexCharFormat& cf,
                              const QHexView* hexview) const;
    virtual void renderHeaderPart(const QString& s, QHexArea area,
                                  QHexCharFormat& cf,
                                  const QHexView* hexview) const;
    virtual bool render(quint64 offset, quint8 b, QHexCharFormat& outcf,
                        const QHexView* hexview) const;
    virtual bool paintSeparator(QPainter* painter, QLineF line,
                                const QHexView* hexview) const;
    virtual void paint(QPainter* painter, const QHexView* hexview) const;
};
