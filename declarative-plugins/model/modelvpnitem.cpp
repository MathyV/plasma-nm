/*
    Copyright 2013 Jan Grulich <jgrulich@redhat.com>

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

#include "model/modelvpnitem.h"

#include "debug.h"

ModelVpnItem::ModelVpnItem(NetworkManager::Device* device, QObject* parent):
    ModelItem(device, parent),
    m_vpn(0)
{
    m_type = NetworkManager::Settings::ConnectionSettings::Vpn;
//     m_icon = "secure-card";
}

ModelVpnItem::~ModelVpnItem()
{
    delete m_vpn;
}

void ModelVpnItem::setActiveConnection(NetworkManager::ActiveConnection* active)
{
    ModelItem::setActiveConnection(active);

    if (m_active->vpn()) {
        m_vpn = new NetworkManager::VpnConnection(m_active->path());

        connect(m_vpn, SIGNAL(stateChanged(NetworkManager::VpnConnection::State)),
                SLOT(onVpnConnectionStateChanged(NetworkManager::VpnConnection::State)), Qt::UniqueConnection);
    }
}

void ModelVpnItem::onVpnConnectionStateChanged(NetworkManager::VpnConnection::State state)
{
    if (state == NetworkManager::VpnConnection::Disconnected) {
        m_connecting = false;
        m_connected = false;
        delete m_vpn;
        m_vpn = 0;
        NMItemDebug() << name() << ": disconnected";
        m_active = 0;
    }
}