/***************************************************************************
 *   Copyright (C) 2005-08 by the Quassel Project                          *
 *   devel@quassel-irc.org                                                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3.                                           *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QFontMetrics>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QTextLayout>

#include "chatitem.h"
#include "chatlinemodel.h"
#include "qtui.h"

ChatItem::ChatItem(int col, QAbstractItemModel *model, QGraphicsItem *parent)
  : QGraphicsItem(parent),
    _fontMetrics(0),
    _col(col),
    _lines(0),
    _layout(0),
    _selectionMode(NoSelection),
    _selectionStart(-1)
{
  Q_ASSERT(model);
  QModelIndex index = model->index(row(), col);
  _fontMetrics = QtUi::style()->fontMetrics(model->data(index, ChatLineModel::FormatRole).value<UiStyle::FormatList>().at(0).second);
  setAcceptHoverEvents(true);
  setZValue(20);
}

ChatItem::~ChatItem() {
  delete _layout;
}

QVariant ChatItem::data(int role) const {
  QModelIndex index = model()->index(row(), column());
  if(!index.isValid()) {
    qWarning() << "ChatItem::data(): model index is invalid!" << index;
    return QVariant();
  }
  return model()->data(index, role);
}

qreal ChatItem::setWidth(qreal w) {
  if(w == _boundingRect.width()) return _boundingRect.height();
  prepareGeometryChange();
  _boundingRect.setWidth(w);
  qreal h = computeHeight();
  _boundingRect.setHeight(h);
  if(haveLayout()) updateLayout();
  return h;
}

qreal ChatItem::computeHeight() {
  if(data(ChatLineModel::ColumnTypeRole).toUInt() != ChatLineModel::ContentsColumn)
    return fontMetrics()->lineSpacing(); // only contents can be multi-line

  _lines = 1;
  WrapColumnFinder finder(this);
  while(finder.nextWrapColumn() > 0) _lines++;
  return _lines * fontMetrics()->lineSpacing();
}

QTextLayout *ChatItem::createLayout(QTextOption::WrapMode wrapMode, Qt::Alignment alignment) {
  QTextLayout *layout = new QTextLayout(data(MessageModel::DisplayRole).toString());

  QTextOption option;
  option.setWrapMode(wrapMode);
  option.setAlignment(alignment);
  layout->setTextOption(option);

  QList<QTextLayout::FormatRange> formatRanges
         = QtUi::style()->toTextLayoutList(data(MessageModel::FormatRole).value<UiStyle::FormatList>(), layout->text().length());
  layout->setAdditionalFormats(formatRanges);
  return layout;
}

void ChatItem::updateLayout() {
  switch(data(ChatLineModel::ColumnTypeRole).toUInt()) {
    case ChatLineModel::TimestampColumn:
      if(!haveLayout()) _layout = createLayout(QTextOption::WrapAnywhere, Qt::AlignLeft);
      // fallthrough
    case ChatLineModel::SenderColumn:
      if(!haveLayout()) _layout = createLayout(QTextOption::WrapAnywhere, Qt::AlignRight);
      _layout->beginLayout();
      {
        QTextLine line = _layout->createLine();
        if(line.isValid()) {
          line.setLineWidth(width());
          line.setPosition(QPointF(0,0));
        }
        _layout->endLayout();
      }
      break;
    case ChatLineModel::ContentsColumn: {
      if(!haveLayout()) _layout = createLayout(QTextOption::WrapAnywhere);

      // Now layout
      ChatLineModel::WrapList wrapList = data(ChatLineModel::WrapListRole).value<ChatLineModel::WrapList>();
      if(!wrapList.count()) return; // empty chatitem

      qreal h = 0;
      WrapColumnFinder finder(this);
      _layout->beginLayout();
      forever {
        QTextLine line = _layout->createLine();
        if(!line.isValid())
          break;

        int col = finder.nextWrapColumn();
        line.setNumColumns(col >= 0 ? col - line.textStart() : _layout->text().length());
        line.setPosition(QPointF(0, h));
        h += line.height() + fontMetrics()->leading();
      }
      _layout->endLayout();
    }
    break;
  }
}

void ChatItem::clearLayout() {
  delete _layout;
  _layout = 0;
}

// NOTE: This is not the most time-efficient implementation, but it saves space by not caching unnecessary data
//       This is a deliberate trade-off. (-> selectFmt creation, data() call)
void ChatItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
  Q_UNUSED(option); Q_UNUSED(widget);
  if(!haveLayout()) updateLayout();
  painter->setClipRect(boundingRect()); // no idea why QGraphicsItem clipping won't work
  //if(_selectionMode == FullSelection) {
    //painter->save();
    //painter->fillRect(boundingRect(), QApplication::palette().brush(QPalette::Highlight));
    //painter->restore();
  //}
  QVector<QTextLayout::FormatRange> formats;
  if(_selectionMode != NoSelection) {
    QTextLayout::FormatRange selectFmt;
    selectFmt.format.setForeground(QApplication::palette().brush(QPalette::HighlightedText));
    selectFmt.format.setBackground(QApplication::palette().brush(QPalette::Highlight));
    if(_selectionMode == PartialSelection) {
      selectFmt.start = qMin(_selectionStart, _selectionEnd);
      selectFmt.length = qAbs(_selectionStart - _selectionEnd);
    } else { // FullSelection
      selectFmt.start = 0;
      selectFmt.length = data(MessageModel::DisplayRole).toString().length();
    }
    formats.append(selectFmt);
  }
  _layout->draw(painter, QPointF(0,0), formats, boundingRect());
}

qint16 ChatItem::posToCursor(const QPointF &pos) {
  if(pos.y() > height()) return data(MessageModel::DisplayRole).toString().length();
  if(pos.y() < 0) return 0;
  if(!haveLayout()) updateLayout();
  for(int l = _layout->lineCount() - 1; l >= 0; l--) {
    QTextLine line = _layout->lineAt(l);
    if(pos.y() >= line.y()) {
      return line.xToCursor(pos.x(), QTextLine::CursorOnCharacter);
    }
  }
  return 0;
}

void ChatItem::setFullSelection() {
  if(_selectionMode != FullSelection) {
    _selectionMode = FullSelection;
    update();
  }
}

void ChatItem::clearSelection() {
  if(_selectionMode != NoSelection) {
    _selectionMode = NoSelection;
    update();
  }
}

void ChatItem::continueSelecting(const QPointF &pos) {
  _selectionMode = PartialSelection;
  _selectionEnd = posToCursor(pos);
  update();
}

void ChatItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
  if(event->buttons() & Qt::LeftButton) {
    if(_selectionMode == NoSelection) {
      chatScene()->setSelectingItem(this);  // removes earlier selection if exists
      _selectionStart = _selectionEnd = posToCursor(event->pos());
      _selectionMode = PartialSelection;
    } else {
      chatScene()->setSelectingItem(0);
      _selectionMode = NoSelection;
      update();
    }
    event->accept();
  } else {
    event->ignore();
  }
}

void ChatItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
  if(contains(event->pos())) {
    qint16 end = posToCursor(event->pos());
    if(end != _selectionEnd) {
      _selectionEnd = end;
      update();
    }
  } else {
    setFullSelection();
    chatScene()->startGlobalSelection(this, event->pos());
  }
}

void ChatItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
  if(_selectionMode != NoSelection) {
    _selectionEnd = posToCursor(event->pos());
    QString selection
        = data(MessageModel::DisplayRole).toString().mid(qMin(_selectionStart, _selectionEnd), qAbs(_selectionStart - _selectionEnd));
    QApplication::clipboard()->setText(selection, QClipboard::Clipboard);  // TODO configure where selections should go
    event->accept();
  } else {
    event->ignore();
  }
}

void ChatItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) {
  // FIXME dirty and fast hack to make http:// urls klickable

  QRegExp regex("\\b((?:h|f)t{1,2}ps?://[^\\s]+)\\b");
  QString str = data(ChatLineModel::DisplayRole).toString();
  int idx = posToCursor(event->pos());
  int mi = 0;
  do {
    mi = regex.indexIn(str, mi);
    if(mi < 0) break;
    if(idx >= mi && idx < mi + regex.matchedLength()) {
      QDesktopServices::openUrl(QUrl(regex.capturedTexts()[1]));
      break;
    }
  } while(mi >= 0);
  event->accept();
}

void ChatItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
  //qDebug() << (void*)this << "entering";

}

void ChatItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
  //qDebug() << (void*)this << "leaving";

}

void ChatItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
  //qDebug() << (void*)this << event->pos();

}


/*************************************************************************************************/

ChatItem::WrapColumnFinder::WrapColumnFinder(ChatItem *_item) : item(_item) {
  wrapList = item->data(ChatLineModel::WrapListRole).value<ChatLineModel::WrapList>();
  wordidx = 0;
  layout = 0;
  lastwrapcol = 0;
  lastwrappos = 0;
  w = 0;
}

ChatItem::WrapColumnFinder::~WrapColumnFinder() {
  delete layout;
}

qint16 ChatItem::WrapColumnFinder::nextWrapColumn() {
  while(wordidx < wrapList.count()) {
    w += wrapList.at(wordidx).width;
    if(w >= item->width()) {
      if(lastwrapcol >= wrapList.at(wordidx).start) {
        // first word, and it doesn't fit
        if(!line.isValid()) {
          layout = item->createLayout(QTextOption::NoWrap);
          layout->beginLayout();
          line = layout->createLine();
          line.setLineWidth(item->width());
          layout->endLayout();
        }
        int idx = line.xToCursor(lastwrappos + item->width(), QTextLine::CursorOnCharacter);
        qreal x = line.cursorToX(idx, QTextLine::Trailing);
        w = w - wrapList.at(wordidx).width - (x - lastwrappos);
        lastwrappos = x;
        lastwrapcol = idx;
        return idx;
      }
      // not the first word, so just wrap before this
      lastwrapcol = wrapList.at(wordidx).start;
      lastwrappos = lastwrappos + w - wrapList.at(wordidx).width;
      w = 0;
      return lastwrapcol;
    }
    w += wrapList.at(wordidx).trailing;
    wordidx++;
  }
  return -1;
}
