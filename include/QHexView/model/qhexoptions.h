#pragma once

#include <QBrush>
#include <QChar>
#include <QColor>
#include <QHash>

namespace QHexFlags {

enum : unsigned int {
    None = (1 << 0),
    HSeparator = (1 << 1),
    VSeparator = (1 << 2),
    StyledHeader = (1 << 3),
    StyledAddress = (1 << 4),
    NoHeader = (1 << 5),
    HighlightAddress = (1 << 6),
    HighlightColumn = (1 << 7),
    PaddedAddress = (1 << 8),

    Separators = HSeparator | VSeparator,
    Styled = StyledHeader | StyledAddress,
};

}

struct QHexCharFormat {
    QBrush background;
    QColor foreground;
    QColor underline;
};

struct QHexOptions {
    // Appearance
    QChar unprintablechar{'.'};
    QChar invalidchar{'?'};
    QString addresslabel{""};
    QString hexlabel;
    QString asciilabel;
    quint64 baseaddress{0};
    unsigned int flags{QHexFlags::None};
    unsigned int linelength{0x10};
    unsigned int addresswidth{0};
    unsigned int grouplength{1};
    int scrollsteps{1};

    // Colors & Styles
    QHash<quint8, QHexCharFormat> bytecolors;
    QColor linealt_background;
    QColor line_background;
    QHexCharFormat header_format;
    QHexCharFormat address_format;
    QHexCharFormat addressheader_format;
    QHexCharFormat hexheader_format;
    QHexCharFormat asciiheader_format;
    QColor comment_color;
    QColor separator_color;

    // Misc
    bool copybreak{true};

    inline bool hasFlag(unsigned int flag) const { return flags & flag; }
};
