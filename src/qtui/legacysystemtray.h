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

#ifndef LEGACYSYSTEMTRAY_H_
#define LEGACYSYSTEMTRAY_H_

#ifndef QT_NO_SYSTEMTRAYICON

#ifdef HAVE_KDE
#  include <KSystemTrayIcon>
#else
#  include <QSystemTrayIcon>
#endif

#include <QTimer>

#include "systemtray.h"

class LegacySystemTray : public SystemTray {
  Q_OBJECT

public:
  LegacySystemTray(QObject *parent = 0);
  virtual ~LegacySystemTray() {}
  virtual void init();

  virtual inline bool isSystemTrayAvailable() const;
  virtual Icon stateIcon() const; // overriden to care about blinkState

public slots:
  virtual void setState(State state);
  virtual void setVisible(bool visible = true);
  virtual void showMessage(const QString &title, const QString &message, MessageIcon icon = Information, int millisecondsTimeoutHint = 10000);

protected:
  virtual void setMode(Mode mode);

private slots:
  void on_blinkTimeout();
  void on_activated(QSystemTrayIcon::ActivationReason);

private:
  void syncLegacyIcon();

  QTimer _blinkTimer;
  bool _blinkState;
  bool _isVisible;

#ifdef HAVE_KDE
  KSystemTrayIcon *_trayIcon;
#else
  QSystemTrayIcon *_trayIcon;
#endif

};

// inlines

bool LegacySystemTray::isSystemTrayAvailable() const { return QSystemTrayIcon::isSystemTrayAvailable(); }

#endif /* QT_NO_SYSTEMTRAYICON */

#endif /* LEGACYSYSTEMTRAY_H_ */