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

#include "CredentialDialog.h"
#include "ui_CredentialDialog.h"

#include "browser/BrowserService.h"
#include "core/Metadata.h"
#include "gui/MainWindow.h"
#include <QCloseEvent>
#include <QFileInfo>

CredentialDialog::CredentialDialog(QWidget* parent)
    : QDialog(parent)
    , m_ui(new Ui::CredentialDialog())
{
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);

    m_ui->setupUi(this);

    connect(this, SIGNAL(updateGroups()), this, SLOT(addGroups()));
    connect(this, SIGNAL(updateEntries()), this, SLOT(addEntries()));
    connect(m_ui->createButton, SIGNAL(clicked()), SLOT(accept()));
    connect(m_ui->cancelButton, SIGNAL(clicked()), SLOT(reject()));
    connect(m_ui->selectDatabaseCombobBox, SIGNAL(currentIndexChanged(int)), SLOT(changeDatabase(int)));
    connect(m_ui->selectEntryComboBox, SIGNAL(currentIndexChanged(int)), SLOT(changeEntry(int)));
    connect(m_ui->selectGroupComboBox, SIGNAL(currentIndexChanged(int)), SLOT(changeGroup(int)));
}

CredentialDialog::~CredentialDialog()
{
}

void CredentialDialog::setInfo(const QString& location,
                               const QString& username,
                               const QSharedPointer<Database>& database,
                               bool isEntry,
                               const QString& titleText,
                               const QString& infoText,
                               const QString& createButtonText,
                               bool isPasskey)
{
    if (isPasskey) {
        setWindowTitle(tr("KeePassXC - Passkey Import"));
        m_ui->infoLabel->setText(tr("Import the following passkey:"));
        m_ui->locationLabel->setText(tr("Relying Party: %1").arg(location));
    } else {
        setWindowTitle(tr("KeePassXC - Create or update credential"));
        m_ui->infoLabel->setText(tr("Create or update the following credential:"));
        m_ui->locationLabel->setText(tr("URL: %1").arg(location));
    }
    m_ui->usernameLabel->setText(tr("Username: %1").arg(username));

    // Only passkeys use this parameter
    if (isEntry) {
        m_ui->verticalLayout->setSizeConstraint(QLayout::SetFixedSize);
        m_ui->infoLabel->setText(tr("Import the following passkey to this entry:"));
        m_ui->groupBox->setVisible(false);
    }

    m_selectedDatabase = database;
    addDatabases();
    addGroups();

    auto openDatabaseCount = 0;
    for (auto dbWidget : getMainWindow()->getOpenDatabases()) {
        if (dbWidget && !dbWidget->isLocked()) {
            openDatabaseCount++;
        }
    }
    m_ui->selectDatabaseCombobBox->setEnabled(openDatabaseCount > 1);

    if (!titleText.isEmpty()) {
        setWindowTitle(titleText);
    }

    if (!infoText.isEmpty()) {
        m_ui->infoLabel->setText(infoText);
    }

    if (!createButtonText.isEmpty()) {
        m_ui->createButton->setText(createButtonText);
    }

    m_isPasskey = isPasskey;
}

QSharedPointer<Database> CredentialDialog::getSelectedDatabase() const
{
    return m_selectedDatabase;
}

QUuid CredentialDialog::getSelectedEntryUuid() const
{
    return m_selectedEntryUuid;
}

QUuid CredentialDialog::getSelectedGroupUuid() const
{
    return m_selectedGroupUuid;
}

bool CredentialDialog::useDefaultGroup() const
{
    return m_selectedGroupUuid.isNull();
}

bool CredentialDialog::createNewEntry() const
{
    return m_selectedEntryUuid.isNull();
}

void CredentialDialog::addDatabases()
{
    auto currentDatabaseIndex = 0;
    const auto openDatabases = browserService()->getOpenDatabases();
    const auto currentDatabase = browserService()->getDatabase();

    m_ui->selectDatabaseCombobBox->clear();
    for (const auto& db : openDatabases) {
        m_ui->selectDatabaseCombobBox->addItem(db->metadata()->name(), db->rootGroup()->uuid());
        if (db->rootGroup()->uuid() == currentDatabase->rootGroup()->uuid()) {
            currentDatabaseIndex = m_ui->selectDatabaseCombobBox->count() - 1;
        }
    }

    m_ui->selectDatabaseCombobBox->setCurrentIndex(currentDatabaseIndex);
}

void CredentialDialog::addEntries()
{
    if (!m_selectedDatabase || !m_selectedDatabase->rootGroup()) {
        return;
    }

    m_ui->selectEntryComboBox->clear();
    m_ui->selectEntryComboBox->addItem(tr("Create new entry"), {});

    const auto group = m_selectedDatabase->rootGroup()->findGroupByUuid(m_selectedGroupUuid);
    if (!group) {
        return;
    }

    // Collect all entries in the group and resolve the title
    QList<QPair<QString, QUuid>> entries;
    for (const auto entry : group->entries()) {
        if (!entry || entry->isRecycled()) {
            continue;
        }
        entries.append({entry->resolveMultiplePlaceholders(entry->title()), entry->uuid()});
    }

    // Sort entries by title
    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
        return a.first.compare(b.first, Qt::CaseInsensitive) < 0;
    });

    // Add sorted entries to the combobox
    for (const auto& pair : entries) {
        m_ui->selectEntryComboBox->addItem(pair.first, pair.second);
    }
}

void CredentialDialog::addGroups()
{
    if (!m_selectedDatabase) {
        return;
    }

    m_ui->selectGroupComboBox->clear();
    if (m_isPasskey) {
        m_ui->selectGroupComboBox->addItem(tr("Default passkeys group (Imported Passkeys)"), {});
    } else {
        m_ui->selectGroupComboBox->addItem(tr("Default browser group"), {});
    }

    for (const auto& group : m_selectedDatabase->rootGroup()->groupsRecursive(true)) {
        if (!group || group->isRecycled() || group == m_selectedDatabase->metadata()->recycleBin()) {
            continue;
        }

        m_ui->selectGroupComboBox->addItem(group->fullPath(), group->uuid());
    }
}

void CredentialDialog::changeDatabase(int index)
{
    m_selectedDatabaseUuid = m_ui->selectDatabaseCombobBox->itemData(index).value<QUuid>();
    m_selectedDatabase = browserService()->getDatabase(m_selectedDatabaseUuid);
    emit updateGroups();
}

void CredentialDialog::changeEntry(int index)
{
    m_selectedEntryUuid = m_ui->selectEntryComboBox->itemData(index).value<QUuid>();
}

void CredentialDialog::changeGroup(int index)
{
    m_selectedGroupUuid = m_ui->selectGroupComboBox->itemData(index).value<QUuid>();
    emit updateEntries();
}
