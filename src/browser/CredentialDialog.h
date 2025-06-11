/*
 *  Copyright (C) 2025 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef KEEPASSXC_CREDENTIALDIALOG_H
#define KEEPASSXC_CREDENTIALDIALOG_H

#include "core/Database.h"
#include "core/Group.h"
#include <QDialog>
#include <QUuid>

namespace Ui
{
    class CredentialDialog;
}

class CredentialDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CredentialDialog(QWidget* parent = nullptr);
    ~CredentialDialog() override;

    void setInfo(const QString& location,
                 const QString& username,
                 const QSharedPointer<Database>& database,
                 bool isEntry,
                 const QString& titleText = {},
                 const QString& infoText = {},
                 const QString& createButtonText = {},
                 bool isPasskey = false);
    QSharedPointer<Database> getSelectedDatabase() const;
    QUuid getSelectedEntryUuid() const;
    QUuid getSelectedGroupUuid() const;
    bool useDefaultGroup() const;
    bool createNewEntry() const;

private:
    void addDatabases();

signals:
    void updateEntries();
    void updateGroups();

private slots:
    void addEntries();
    void addGroups();
    void changeDatabase(int index);
    void changeEntry(int index);
    void changeGroup(int index);

private:
    QScopedPointer<Ui::CredentialDialog> m_ui;
    QSharedPointer<Database> m_selectedDatabase;
    QUuid m_selectedDatabaseUuid;
    QUuid m_selectedEntryUuid;
    QUuid m_selectedGroupUuid;
    bool m_isPasskey;
};

#endif // KEEPASSXC_CREDENTIALDIALOG_H
