#pragma once

#include <QList>

struct QHexChangeRange {
    qint64 start;
    qint64 end;

    bool operator<(const QHexChangeRange& rhs) const {
        return start < rhs.start;
    }
};

using QHexChanges = QList<QHexChangeRange>;
