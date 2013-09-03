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

#include "connectionsettingshandler.h"

#include <NetworkManagerQt/AccessPoint>
#include <NetworkManagerQt/WirelessDevice>
#include <NetworkManagerQt/Manager>
#include <NetworkManagerQt/Settings>
#include <NetworkManagerQt/ConnectionSettings>
#include <NetworkManagerQt/Ipv4Setting>
#include <NetworkManagerQt/Setting>
#include <NetworkManagerQt/WirelessSetting>
#include <NetworkManagerQt/WirelessSecuritySetting>
#include <NetworkManagerQt/WiredSetting>

ConnectionSettingsHandler::ConnectionSettingsHandler(QObject* parent)
    : QObject(parent)
{
}

ConnectionSettingsHandler::~ConnectionSettingsHandler()
{
}

QVariantMap ConnectionSettingsHandler::loadSettings(const QString& uuid)
{
    QVariantMap resultingMap;

    if (uuid.isEmpty()) {
        return resultingMap;
    }

    NetworkManager::Connection::Ptr connection = NetworkManager::findConnectionByUuid(uuid);

    if (!connection) {
        return resultingMap;
    }

    connect(connection.data(), SIGNAL(gotSecrets(QString,bool,NMVariantMapMap,QString)),
            SLOT(gotSecrets(QString,bool,NMVariantMapMap,QString)), Qt::UniqueConnection);

    NetworkManager::ConnectionSettings::Ptr connectionsettings = connection->settings();

    if (!connectionsettings) {
        return resultingMap;
    } else {
        QVariantMap connectionMap;

        connectionMap.insert("id", connectionsettings->id());
        connectionMap.insert("autoconnect", connectionsettings->autoconnect());

        resultingMap.insert("connection", connectionMap);
    }

    NetworkManager::Ipv4Setting::Ptr ipv4Setting = connectionsettings->setting(NetworkManager::Setting::Ipv4).staticCast<NetworkManager::Ipv4Setting>();

    if (ipv4Setting) {
        QVariantMap ipv4map;

        if (ipv4Setting->method() == NetworkManager::Ipv4Setting::Automatic) {
            ipv4map.insert("method", "auto");
        } else if (ipv4Setting->method() == NetworkManager::Ipv4Setting::Shared) {
            ipv4map.insert("method", "shared");
        } else if (ipv4Setting->method() == NetworkManager::Ipv4Setting::Manual) {
            ipv4map.insert("method", "manual");

            if (!ipv4Setting->addresses().isEmpty()) {
                NetworkManager::IpAddress ipAddress = ipv4Setting->addresses().first();
                if (ipAddress.isValid()) {
                    ipv4map.insert("address", ipAddress.ip().toString());
                    ipv4map.insert("gateway", ipAddress.gateway().toString());
                    ipv4map.insert("netmask", ipAddress.netmask().toString());
                }
            }

            if (!ipv4Setting->dns().isEmpty()) {
                QHostAddress dns1 = ipv4Setting->dns().first();
                ipv4map.insert("dns1", dns1.toString());
                if (ipv4Setting->dns().count() >= 2) {
                    QHostAddress dns2 = ipv4Setting->dns().at(1);
                    ipv4map.insert("dns2", dns2.toString());
                }
            }
        }

        resultingMap.insert("ipv4", QVariant::fromValue(ipv4map));
    }

    if (connectionsettings->connectionType() == NetworkManager::ConnectionSettings::Wireless) {
        NetworkManager::WirelessSetting::Ptr wirelessSetting = connectionsettings->setting(NetworkManager::Setting::Wireless).staticCast<NetworkManager::WirelessSetting>();

        if (wirelessSetting) {
            QVariantMap wirelessMap;

            wirelessMap.insert("ssid", wirelessSetting->ssid());
            if (wirelessSetting->mode() == NetworkManager::WirelessSetting::Infrastructure) {
                wirelessMap.insert("mode", "infrastructure");
            } else if (wirelessSetting->mode() == NetworkManager::WirelessSetting::Adhoc) {
                wirelessMap.insert("mode", "adhoc");
            } else if (wirelessSetting->mode() == NetworkManager::WirelessSetting::Ap) {
                wirelessMap.insert("mode", "ap");
            }

            resultingMap.insert("802-11-wireless", wirelessMap);

            if (wirelessSetting->security() == "802-11-wireless-security") {
                NetworkManager::WirelessSecuritySetting::Ptr wirelessSecuritySetting = connectionsettings->setting(NetworkManager::Setting::WirelessSecurity).staticCast<NetworkManager::WirelessSecuritySetting>();

                if (wirelessSecuritySetting) {
                    QVariantMap wifiSecurityMap;

                    if (wirelessSecuritySetting->keyMgmt() == NetworkManager::WirelessSecuritySetting::Wep) {
                        connection->secrets("802-11-wireless-security");
                        wifiSecurityMap.insert("key-mgmt", "none");
                        // TODO: maybe check wep-key index
                        wifiSecurityMap.insert("wep-key0", wirelessSecuritySetting->wepKey0());
                    } else if (wirelessSecuritySetting->keyMgmt() == NetworkManager::WirelessSecuritySetting::Ieee8021x) {
                        wifiSecurityMap.insert("key-mgmt", "ieee8021x");
                        if (wirelessSecuritySetting->authAlg() == NetworkManager::WirelessSecuritySetting::Leap) {
                            connection->secrets("802-11-wireless-security");
                            wifiSecurityMap.insert("auth-alg", "leap");
                            wifiSecurityMap.insert("leap-username", wirelessSecuritySetting->leapUsername());
                            wifiSecurityMap.insert("leap-password", wirelessSecuritySetting->leapPassword());
                        } else {
                            connection->secrets("802-1x");
                        }
                    } else if (wirelessSecuritySetting->keyMgmt() == NetworkManager::WirelessSecuritySetting::WpaNone) {
                        connection->secrets("802-11-wireless-security");
                        wifiSecurityMap.insert("key-mgmt", "wpa-none");
                        wifiSecurityMap.insert("psk", wirelessSecuritySetting->psk());
                    } else if (wirelessSecuritySetting->keyMgmt() == NetworkManager::WirelessSecuritySetting::WpaPsk) {
                        connection->secrets("802-11-wireless-security");
                        wifiSecurityMap.insert("key-mgmt", "wpa-psk");
                        wifiSecurityMap.insert("psk", wirelessSecuritySetting->psk());
                    } else if (wirelessSecuritySetting->keyMgmt() == NetworkManager::WirelessSecuritySetting::WpaEap) {
                        connection->secrets("802-1x");
                        wifiSecurityMap.insert("key-mgmt", "wpa-eap");
                        // TODO
                    }

                    resultingMap.insert("802-11-wireless-security", wifiSecurityMap);
                }
            }
        }
    }

    return resultingMap;
}

void ConnectionSettingsHandler::addConnection(const QVariantMap& map)
{
//     qDebug() << "INPUT";
//     qDebug() << map;
//     qDebug() << "OUTPUT";
    qDebug() << nmVariantMapMap(map);
    NetworkManager::addConnection(nmVariantMapMap(map));
}

void ConnectionSettingsHandler::addAndActivateConnection(const QVariantMap& map, const QString& device, const QString& specificPath)
{
    NetworkManager::AccessPoint::Ptr ap;
    NetworkManager::WirelessDevice::Ptr wifiDev;
    foreach (const NetworkManager::Device::Ptr & dev, NetworkManager::networkInterfaces()) {
        if (dev->type() == NetworkManager::Device::Wifi) {
            wifiDev = dev.objectCast<NetworkManager::WirelessDevice>();
            ap = wifiDev->findAccessPoint(specificPath);
            if (ap) {
                break;
            }
        }
    }

    if (!ap) {
        return;
    }

    NetworkManager::ConnectionSettings connectionSettings;
    connectionSettings.fromMap(nmVariantMapMap(map));

    NetworkManager::WirelessSetting::Ptr wifiSetting = connectionSettings.setting(NetworkManager::Setting::Wireless).dynamicCast<NetworkManager::WirelessSetting>();
    if (ap->mode() == NetworkManager::AccessPoint::Adhoc) {
        wifiSetting->setMode(NetworkManager::WirelessSetting::Adhoc);
    }
    NetworkManager::addAndActivateConnection(connectionSettings.toMap(), device, specificPath);
}

void ConnectionSettingsHandler::gotSecrets(const QString& id, bool success, const NMVariantMapMap& secrets, const QString& msg)
{
    if (success) {
        QVariantMap resultingMap;

        foreach (const QString & key, secrets.keys()) {
            resultingMap.insert(key, secrets.value(key));
        }

        emit loadSecrets(resultingMap);
    } else {
        qDebug() << "Failed to retrive secrets for " << id;
        qDebug() << "Reason: " << msg;
    }
}

void ConnectionSettingsHandler::saveSettings(const QVariantMap& map, const QString& connection)
{
//     qDebug() << "INPUT";
//     qDebug() << map;
//     qDebug() << "OUTPUT";
//     qDebug() << nmVariantMapMap(map);
    NetworkManager::Connection::Ptr con = NetworkManager::findConnection(connection);
    con->update(nmVariantMapMap(map));
}

NMVariantMapMap ConnectionSettingsHandler::nmVariantMapMap(const QVariantMap& map)
{
    NetworkManager::ConnectionSettings connectionSettings;
    if (map.contains("connection")) {
        QVariantMap connectionMap = map.value("connection").toMap();
        connectionSettings.setConnectionType((NetworkManager::ConnectionSettings::ConnectionType)connectionMap.value("type").toInt());
        connectionSettings.setId(connectionMap.value("id").toString());
        connectionSettings.setAutoconnect(connectionMap.value("autoconnect").toBool());
        if (connectionMap.contains("uuid")) {
            connectionSettings.setUuid(connectionMap.value("uuid").toString());
        } else {
            connectionSettings.setUuid(NetworkManager::ConnectionSettings::createNewUuid());
        }

        // We have to pass at least an empty wired setting
        if (connectionSettings.connectionType() == NetworkManager::ConnectionSettings::Wired) {
            NetworkManager::WiredSetting::Ptr wiredSetting = connectionSettings.setting(NetworkManager::Setting::Wired).staticCast<NetworkManager::WiredSetting>();
            wiredSetting->setInitialized(true);
        }
    }

    if (map.contains("ipv4")) {
        NetworkManager::Ipv4Setting::Ptr ipv4Setting = connectionSettings.setting(NetworkManager::Setting::Ipv4).staticCast<NetworkManager::Ipv4Setting>();
        QVariantMap ipv4Map = map.value("ipv4").toMap();
        QString method = ipv4Map.value("method").toString();
        if (method == "auto") {
            ipv4Setting->setMethod(NetworkManager::Ipv4Setting::Automatic);
        } else if (method == "shared") {
            ipv4Setting->setMethod(NetworkManager::Ipv4Setting::Shared);
        } else if (method == "manual") {
            bool valid = false;
            NetworkManager::IpAddress ipAddr;
            QList<QHostAddress> dnsServers;
            if (ipv4Map.contains("address") && ipv4Map.contains("netmask")) {
                ipAddr.setIp(QHostAddress(ipv4Map.value("address").toString()));
                ipAddr.setNetmask(QHostAddress(ipv4Map.value("netmask").toString()));
                if (ipv4Map.contains("gateway")) {
                    ipAddr.setGateway(QHostAddress(ipv4Map.value("gateway").toString()));
                }
                if (ipv4Map.contains("dns1")) {
                    dnsServers << QHostAddress(ipv4Map.value("dns1").toString());
                }
                if (ipv4Map.contains("dns2")) {
                    dnsServers << QHostAddress(ipv4Map.value("dns2").toString());
                }
            }

            if (ipAddr.isValid()) {
                NetworkManager::IpAddresses addresses;
                addresses << ipAddr;
                ipv4Setting->setAddresses(addresses);
                valid = true;
            }

            if (!dnsServers.isEmpty()) {
                ipv4Setting->setDns(dnsServers);
            }

            if (valid) {
                ipv4Setting->setMethod(NetworkManager::Ipv4Setting::Manual);
            } else {
                if (dnsServers.isEmpty()) {
                    ipv4Setting->setMethod(NetworkManager::Ipv4Setting::Automatic);
                } else {
                    ipv4Setting->setMethod(NetworkManager::Ipv4Setting::Automatic);
                    ipv4Setting->setIgnoreAutoDns(true);
                }
            }
        }
        ipv4Setting->setInitialized(true);
    }

    if (map.contains("802-11-wireless")) {
        NetworkManager::WirelessSetting::Ptr wirelessSetting = connectionSettings.setting(NetworkManager::Setting::Wireless).staticCast<NetworkManager::WirelessSetting>();
        QVariantMap wirelessMap = map.value("802-11-wireless").toMap();
        wirelessSetting->setSsid(wirelessMap.value("ssid").toString().toUtf8());
        if (wirelessMap.value("mode").toString() == "infrastructure") {
            wirelessSetting->setMode(NetworkManager::WirelessSetting::Infrastructure);
        } else if (wirelessMap.value("mode").toString() == "adhoc") {
            wirelessSetting->setMode(NetworkManager::WirelessSetting::Adhoc);
        } else {
            wirelessSetting->setMode(NetworkManager::WirelessSetting::Ap);
        }
        if (map.contains("802-11-wireless-security") && map.value("802-11-wireless-security").toMap().contains("key-mgmt")) {
            wirelessSetting->setSecurity("802-11-wireless-security");
        }
        wirelessSetting->setInitialized(true);
    }

    if (map.contains("802-11-wireless-security") && map.value("802-11-wireless-security").toMap().contains("key-mgmt")) {
        NetworkManager::WirelessSecuritySetting::Ptr wirelessSecuritySetting = connectionSettings.setting(NetworkManager::Setting::WirelessSecurity).staticCast<NetworkManager::WirelessSecuritySetting>();
        QVariantMap wirelessSecurityMap = map.value("802-11-wireless-security").toMap();
        QString keymgmt = wirelessSecurityMap.value("key-mgmt").toString();
        if (keymgmt == "none") {
            wirelessSecuritySetting->setKeyMgmt(NetworkManager::WirelessSecuritySetting::Wep);
            wirelessSecuritySetting->setWepKey0(wirelessSecurityMap.value("wep-key0").toString());
            wirelessSecuritySetting->setWepKeyFlags(NetworkManager::Setting::AgentOwned);
        } else if (keymgmt == "ieee8021x") {
            if (wirelessSecurityMap.contains("auth-alg") && wirelessSecurityMap.value("auth-alg").toString() == "leap") {
                wirelessSecuritySetting->setKeyMgmt(NetworkManager::WirelessSecuritySetting::Ieee8021x);
                wirelessSecuritySetting->setAuthAlg(NetworkManager::WirelessSecuritySetting::Leap);
                wirelessSecuritySetting->setLeapUsername(wirelessSecurityMap.value("leap-username").toString());
                wirelessSecuritySetting->setLeapPassword(wirelessSecurityMap.value("leap-password").toString());
                wirelessSecuritySetting->setLeapPasswordFlags(NetworkManager::Setting::AgentOwned);
            }
            // TODO
        } else if (keymgmt == "wpa-none") {
            wirelessSecuritySetting->setKeyMgmt(NetworkManager::WirelessSecuritySetting::WpaNone);
            wirelessSecuritySetting->setPsk(wirelessSecurityMap.value("psk").toString());
            wirelessSecuritySetting->setPskFlags(NetworkManager::Setting::AgentOwned);
        } else if (keymgmt == "wpa-psk") {
            wirelessSecuritySetting->setKeyMgmt(NetworkManager::WirelessSecuritySetting::WpaPsk);
            wirelessSecuritySetting->setPsk(wirelessSecurityMap.value("psk").toString());
            wirelessSecuritySetting->setPskFlags(NetworkManager::Setting::AgentOwned);
        } else if (keymgmt == "wpa-eap") {
            // TODO
        }
        wirelessSecuritySetting->setInitialized(true);
    }

    return connectionSettings.toMap();
}

