#include <QBuffer>
#include <QFile>
#include <QHexView/model/buffer/qdevicebuffer.h>
#include <QHexView/model/buffer/qmappedfilebuffer.h>
#include <QHexView/model/buffer/qmemorybuffer.h>
#include <QHexView/model/commands/insertcommand.h>
#include <QHexView/model/commands/removecommand.h>
#include <QHexView/model/commands/replacecommand.h>
#include <QHexView/model/qhexdocument.h>
#include <cmath>

QHexDocument::QHexDocument(QHexBuffer* buffer, QObject* parent)
    : QObject(parent) {
    m_buffer = buffer;
    m_buffer->setParent(this); // Take Ownership

    m_undostack = new QUndoStack(this);

    connect(m_undostack, &QUndoStack::canUndoChanged, this,
            &QHexDocument::canUndoChanged);
    connect(m_undostack, &QUndoStack::canRedoChanged, this,
            &QHexDocument::canRedoChanged);
    connect(m_undostack, &QUndoStack::cleanChanged, this,
            [&](bool clean) { Q_EMIT modifiedChanged(!clean); });
}

qint64 QHexDocument::indexOf(const QByteArray& ba, qint64 from) {
    return m_buffer->indexOf(ba, from);
}

qint64 QHexDocument::lastIndexOf(const QByteArray& ba, qint64 from) {
    return m_buffer->lastIndexOf(ba, from);
}

bool QHexDocument::isOffsetChanged(qint64 offset) const {
    if(!m_trackchanges)
        return false;

    int left = 0, right = m_changes.size() - 1;

    while(left <= right) {
        int mid = (left + right) / 2;
        const QHexChangeRange& r = m_changes[mid];

        if(offset < r.start)
            right = mid - 1;
        else if(offset >= r.end)
            left = mid + 1;
        else
            return true; // found
    }

    return false;
}

bool QHexDocument::accept(qint64 idx) const { return m_buffer->accept(idx); }
bool QHexDocument::isEmpty() const { return m_buffer->isEmpty(); }
bool QHexDocument::isModified() const { return !m_undostack->isClean(); }
bool QHexDocument::canUndo() const { return m_undostack->canUndo(); }
bool QHexDocument::canRedo() const { return m_undostack->canRedo(); }

bool QHexDocument::trackChanges() const { return m_trackchanges; }

void QHexDocument::setData(const QByteArray& ba) {
    QHexBuffer* mb = new QMemoryBuffer();
    mb->read(ba);
    this->setData(mb);
}

void QHexDocument::setData(QHexBuffer* buffer) {
    if(!buffer)
        return;

    m_undostack->clear();
    buffer->setParent(this);

    auto* oldbuffer = m_buffer;
    m_buffer = buffer;
    if(oldbuffer)
        oldbuffer->deleteLater();

    Q_EMIT canUndoChanged(false);
    Q_EMIT canRedoChanged(false);
    Q_EMIT changed();
    Q_EMIT reset();
}

void QHexDocument::setTrackChanges(bool b) {
    if(b == m_trackchanges)
        return;

    m_trackchanges = b;
    Q_EMIT trackChangesChanged(b);
}

void QHexDocument::clearChanges() {
    if(!m_trackchanges || m_changes.isEmpty())
        return;

    m_changes.clear();
    Q_EMIT changed();
}

void QHexDocument::clearModified() { m_undostack->setClean(); }

qint64 QHexDocument::length() const {
    return m_buffer ? m_buffer->length() : 0;
}

uchar QHexDocument::at(int offset) const { return m_buffer->at(offset); }

QHexDocument* QHexDocument::fromFile(QString filename, QObject* parent) {
    QFile f(filename);
    f.open(QFile::ReadOnly);
    return QHexDocument::fromMemory<QMemoryBuffer>(f.readAll(), parent);
}

void QHexDocument::undo() {
    m_undostack->undo();
    this->restoreChanges();
    Q_EMIT changed();
}

void QHexDocument::redo() {
    m_undostack->redo();
    this->restoreChanges();
    Q_EMIT changed();
}

void QHexDocument::insert(qint64 offset, uchar b) {
    this->insert(offset, QByteArray(1, b));
}

void QHexDocument::replace(qint64 offset, uchar b) {
    this->replace(offset, QByteArray(1, b));
}

void QHexDocument::insert(qint64 offset, const QByteArray& data) {
    m_undostack->push(
        new QHexViewInsertCommand(m_buffer, m_changes, this, offset, data));

    if(m_trackchanges) {
        m_changes.push_back({offset, offset + data.size()});
        std::sort(m_changes.begin(), m_changes.end());
        this->mergeChanges();
    }

    Q_EMIT changed();
    Q_EMIT dataChanged(data, offset, ChangeReason::Insert);
}

void QHexDocument::replace(qint64 offset, const QByteArray& data) {
    m_undostack->push(
        new QHexViewReplaceCommand(m_buffer, m_changes, this, offset, data));

    if(m_trackchanges) {
        m_changes.push_back({offset, offset + data.size()});
        std::sort(m_changes.begin(), m_changes.end());
        this->mergeChanges();
    }

    Q_EMIT changed();
    Q_EMIT dataChanged(data, offset, ChangeReason::Replace);
}

void QHexDocument::remove(qint64 offset, int len) {
    QByteArray data = m_buffer->read(offset, len);
    m_undostack->push(
        new QHexViewRemoveCommand(m_buffer, m_changes, this, offset, len));

    if(m_trackchanges)
        this->removeChange(offset, len);

    Q_EMIT changed();
    Q_EMIT dataChanged(data, offset, ChangeReason::Remove);
}

QByteArray QHexDocument::read(qint64 offset, int len) const {
    return m_buffer->read(offset, len);
}

bool QHexDocument::saveTo(QIODevice* device) {
    if(!device->isWritable())
        return false;
    m_buffer->write(device);
    return true;
}

void QHexDocument::removeChange(qint64 offset, qint64 length) {
    QHexChanges newchanges;

    for(const QHexChangeRange& cr : m_changes) {
        if(cr.end <= offset)
            newchanges.push_back(cr); // before removed range
        else if(cr.start >= offset + length) {
            newchanges.push_back(
                {cr.start - length, cr.end - length}); // after: shift back
        }
        else { // overlaps
            if(cr.start < offset)
                newchanges.push_back({cr.start, offset}); // left part
            if(cr.end > offset + length)
                newchanges.push_back({offset, cr.end - length}); // right part
        }
    }

    m_changes.swap(newchanges);
}

void QHexDocument::mergeChanges() {
    if(m_changes.isEmpty())
        return;

    QHexChanges newchanges;
    QHexChangeRange current = m_changes.first();

    for(int i = 1; i < m_changes.size(); ++i) {
        const QHexChangeRange& next = m_changes[i];

        if(current.end >= next.start)
            current.end = qMax(current.end, next.end);
        else {
            newchanges.push_back(current);
            current = next;
        }
    }

    newchanges.push_back(current);
    m_changes.swap(newchanges);
}

void QHexDocument::restoreChanges() {
    if(!m_trackchanges)
        return;

    const QUndoCommand* cmd = m_undostack->command(m_undostack->index());

    if(cmd)
        m_changes = static_cast<const QHexViewCommand*>(cmd)->changes();
    else
        m_changes.clear();
}

QHexDocument* QHexDocument::fromBuffer(QHexBuffer* buffer, QObject* parent) {
    return new QHexDocument(buffer, parent);
}

QHexDocument* QHexDocument::fromLargeFile(QString filename, QObject* parent) {
    return QHexDocument::fromDevice<QDeviceBuffer>(new QFile(filename), parent);
}

QHexDocument* QHexDocument::fromMappedFile(QString filename, QObject* parent) {
    return QHexDocument::fromDevice<QMappedFileBuffer>(new QFile(filename),
                                                       parent);
}

QHexDocument* QHexDocument::create(QObject* parent) {
    return QHexDocument::fromMemory<QMemoryBuffer>({}, parent);
}
