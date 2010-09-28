/***************************************************************************
 *   Copyright (C) 2005-2010 by the Quassel Project                        *
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

#include "messageevent.h"


MessageEvent::MessageEvent(Message::Type msgType, Network *net, const QString &msg, const QString &target, const QString &sender, Message::Flags flags)
    : NetworkEvent(EventManager::MessageEvent, net),
      _msgType(msgType),
      _text(msg),
      _target(target),
      _sender(sender),
      _msgFlags(flags)
{
  IrcChannel *channel = network()->ircChannel(_target);
  if(!channel) {
    if(!_target.isEmpty() && network()->prefixes().contains(_target.at(0)))
      _target = _target.mid(1);

    if(_target.startsWith('$') || _target.startsWith('#'))
      _target = nickFromMask(sender);
  }

  _bufferType = bufferTypeByTarget(_target);
}

BufferInfo::Type MessageEvent::bufferTypeByTarget(const QString &target) const {
  if(target.isEmpty())
    return BufferInfo::StatusBuffer;

  if(network()->isChannelName(target))
    return BufferInfo::ChannelBuffer;

  return BufferInfo::QueryBuffer;
}
