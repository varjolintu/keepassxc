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

#ifndef KEEPASSX_SECURELINEEDIT_H
#define KEEPASSX_SECURELINEEDIT_H

#include <QMacCocoaViewContainer>
#include <QLineEdit>

#import <AppKit/NSSecureTextField.h>

class QToolButton;

#ifdef __OBJC__
@class NSSecureTextField;
typedef NSSecureTextField *NativeNSSecureTextFieldRef;
typedef const NSSecureTextField *ConstNativeNSSecureTextFieldRef;
#else
typedef void* NativeNSSecureTextFieldRef;
typedef const void* ConstNativeNSSecureTextFieldRef;
#endif

class SecureLineEditWrapper : public QMacCocoaViewContainer
{
    Q_OBJECT
public:
    explicit SecureLineEditWrapper(QWidget *parent = 0);
    //void handleClicked();

signals:
    //void clicked();

public slots:

public:
    NativeNSSecureTextFieldRef m_ref;
};

class SecureLineEdit : public QLineEdit
{
    Q_OBJECT

public:
    explicit SecureLineEdit(QWidget* parent = nullptr);

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void updateCloseButton(const QString& text);

private:
    QToolButton* const m_clearButton;
    SecureLineEditWrapper* m_wrapper;
};

#endif // KEEPASSX_SECURELINEEDIT_H
