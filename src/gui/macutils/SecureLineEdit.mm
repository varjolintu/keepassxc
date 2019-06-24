/*
 *  Copyright (C) 2007 Trolltech ASA <info@trolltech.com>
 *  Copyright (C) 2012 Felix Geyer <debfx@fobos.de>
 *  Copyright (C) 2012 Florian Geyer <blueice@fobos.de>
 *  Copyright (C) 2019 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 or (at your option)
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "SecureLineEdit.h"

#include <QStyle>
#include <QToolButton>
#include <QResizeEvent>

#include "core/FilePath.h"

#import <Cocoa/Cocoa.h>

@interface SecureLineEditWrapperImpl: NSObject
{
    SecureLineEditWrapper* wrapper;
}

- (void)controlTextDidChange:(NSNotification *)obj;

@end

@implementation SecureLineEditWrapperImpl

- (void)controlTextDidChange:(NSNotification *)obj
{
    NSSecureTextField *textField = [obj object];
    NSString *value = [textField stringValue];
    wrapper->handleTextChanged(QString::fromNSString(value));
}

SecureLineEditWrapper::SecureLineEditWrapper(QWidget* parent)
    : QMacCocoaViewContainer(nullptr, parent)
{
    self = [[SecureLineEditWrapperImpl alloc] init];

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    m_ref = [[NSSecureTextField alloc] init];
    setCocoaView(m_ref);

    [[NSNotificationCenter defaultCenter] addObserver:static_cast<id>(self)
                                          selector:@selector(controlTextDidChange:)
                                          name:NSControlTextDidChangeNotification
                                          object:nil];

    [m_ref release];
    [pool release];
}

SecureLineEditWrapper::~SecureLineEditWrapper()
{
    [[NSNotificationCenter defaultCenter] removeObserver:static_cast<id>(m_ref)];
    [static_cast<id>(self) dealloc];
}

void SecureLineEditWrapper::handleTextChanged(QString value)
{
    Q_UNUSED(value);
    qDebug("Text changed.");
    //emit textChanged(value); // Emit signal to QLineEdit (Crashes)
}

@end

SecureLineEdit::SecureLineEdit(QWidget* parent)
    : QLineEdit(parent)
    , m_clearButton(new QToolButton(this))
{
    m_wrapper = new SecureLineEditWrapper(parent);

    m_clearButton->setObjectName("clearButton");

    QIcon icon;
    QString iconNameDirected =
        QString("edit-clear-locationbar-").append((layoutDirection() == Qt::LeftToRight) ? "rtl" : "ltr");
    icon = QIcon::fromTheme(iconNameDirected);
    if (icon.isNull()) {
        icon = QIcon::fromTheme("edit-clear");
        if (icon.isNull()) {
            icon = filePath()->icon("actions", iconNameDirected);
        }
    }

    m_clearButton->setIcon(icon);
    m_clearButton->setCursor(Qt::ArrowCursor);
    m_clearButton->setStyleSheet("QToolButton { border: none; padding: 0px; }");
    m_clearButton->hide();
    connect(m_clearButton, SIGNAL(clicked()), this, SLOT(clear()));
    connect(this, SIGNAL(textChanged(QString)), this, SLOT(updateCloseButton(QString)));
    int frameWidth = style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
    setStyleSheet(
        QString("QLineEdit { padding-right: %1px; } ").arg(m_clearButton->sizeHint().width() + frameWidth + 1));
    QSize msz = minimumSizeHint();
    setMinimumSize(qMax(msz.width(), m_clearButton->sizeHint().height() + frameWidth * 2 + 2),
                   qMax(msz.height(), m_clearButton->sizeHint().height() + frameWidth * 2 + 2));
}

void SecureLineEdit::moveEvent(QMoveEvent *event)
{
    // TODO: Fix this.
    /*NSRect frame;
    frame = [m_wrapper->m_ref frame];
    frame.origin.x = event->pos().x();
    frame.origin.y = event->pos().y();
    [m_wrapper->m_ref setFrame:frame];*/

    QLineEdit::moveEvent(event);
}

void SecureLineEdit::resizeEvent(QResizeEvent* event)
{
    QSize sz = m_clearButton->sizeHint();
    int frameWidth = style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
    int y = (rect().bottom() + 1 - sz.height()) / 2;

    if (layoutDirection() == Qt::LeftToRight) {
        m_clearButton->move(rect().right() - frameWidth - sz.width(), y);
    } else {
        m_clearButton->move(rect().left() + frameWidth, y);
    }

    NSRect frame;
    frame = [m_wrapper->m_ref frame];
    frame.size.width = event->size().width();
    frame.size.height = event->size().height();
    [m_wrapper->m_ref setFrame:frame];

    QLineEdit::resizeEvent(event);
}

void SecureLineEdit::updateCloseButton(const QString& text)
{
    m_clearButton->setVisible(!text.isEmpty());
}
