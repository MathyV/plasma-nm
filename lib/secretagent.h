/*
    Copyright 2013 Jan Grulich <jgrulich@redhat.com>
    Copyright 2013 Lukas Tinkl <ltinkl@redhat.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) version 3, or any
    later version accepted by the membership of KDE e.V. (or its
    successor approved by the membership of KDE e.V.), which shall
    act as a proxy defined in Section 6 of version 3 of the license.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef PLASMA_NM_SECRET_AGENT_H
#define PLASMA_NM_SECRET_AGENT_H

#include <QtNetworkManager/secretagent.h>

#include <kdemacros.h>

class KDE_EXPORT SecretAgent : public NetworkManager::SecretAgent
{
    Q_OBJECT
public:
    SecretAgent(QObject* parent = 0);
    virtual ~SecretAgent();

public Q_SLOTS:
    virtual QVariantMapMap GetSecrets(const QVariantMapMap&, const QDBusObjectPath&, const QString&, const QStringList&, uint);
    virtual void SaveSecrets(const QVariantMapMap&, const QDBusObjectPath&);
    virtual void DeleteSecrets(const QVariantMapMap &, const QDBusObjectPath &);
    virtual void CancelGetSecrets(const QDBusObjectPath &, const QString &);
};

#endif // PLASMA_NM_SECRET_AGENT_H