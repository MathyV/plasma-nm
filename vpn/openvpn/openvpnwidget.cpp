/*
    Copyright 2008 Will Stephenson <wstephenson@kde.org>
    Copyright 2013 Lukáš Tinkl <ltinkl@redhat.com>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of
    the License or (at your option) version 3 or any later version
    accepted by the membership of KDE e.V. (or its successor approved
    by the membership of KDE e.V.), which shall act as a proxy
    defined in Section 14 of version 3 of the license.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "debug.h"
#include "openvpnwidget.h"
#include "openvpnadvancedwidget.h"

#include <QDBusMetaType>
#include <QLineEdit>
#include <QUrl>
#include <QPointer>

#include <KProcess>
#include <KUrlRequester>

#include "nm-openvpn-service.h"

class OpenVpnSettingWidget::Private
{
public:
    Ui_OpenVPNProp ui;
    NetworkManager::VpnSetting::Ptr setting;
    class EnumConnectionType
    {
    public:
        enum ConnectionType {Certificates = 0, Psk, Password, CertsPassword};
    };
    class EnumKeyDirection
    {
    public:
        enum KeyDirection {None = 0, D0, D1};
    };
};


OpenVpnSettingWidget::OpenVpnSettingWidget(const NetworkManager::VpnSetting::Ptr &setting, QWidget *parent)
    : SettingWidget(setting, parent)
    , d(new Private)
{
    qDBusRegisterMetaType<NMStringMap>();

    d->ui.setupUi(this);
    d->setting = setting;

    d->ui.x509KeyPassword->setPasswordOptionsEnabled(true);
    d->ui.x509KeyPassword->setPasswordOptionEnabled(PasswordField::NotRequired, true);
    d->ui.passPassword->setPasswordOptionsEnabled(true);
    d->ui.passPassword->setPasswordOptionEnabled(PasswordField::NotRequired, true);
    d->ui.x509PassKeyPassword->setPasswordOptionsEnabled(true);
    d->ui.x509PassKeyPassword->setPasswordOptionEnabled(PasswordField::NotRequired, true);
    d->ui.x509PassPassword->setPasswordOptionsEnabled(true);
    d->ui.x509PassPassword->setPasswordOptionEnabled(PasswordField::NotRequired, true);

    // use requesters' urlSelected signals to set other requester's startDirs to save clicking
    // around the filesystem
    QList<const KUrlRequester *> requesters;
    requesters << d->ui.x509CaFile << d->ui.x509Cert << d->ui.x509Key << d->ui.pskSharedKey << d->ui.passCaFile
               << d->ui.x509PassCaFile << d->ui.x509PassCert << d->ui.x509PassKey;
    Q_FOREACH (const KUrlRequester * requester, requesters) {
        connect(requester, &KUrlRequester::urlSelected, this, &OpenVpnSettingWidget::updateStartDir);
    }

    connect(d->ui.btnAdvanced, &QPushButton::clicked, this, &OpenVpnSettingWidget::showAdvanced);

    connect(d->ui.gateway, &QLineEdit::textChanged, this, &OpenVpnSettingWidget::slotWidgetChanged);

    KAcceleratorManager::manage(this);

    if (setting && !setting->isNull()) {
        loadConfig(d->setting);
    }
}

OpenVpnSettingWidget::~OpenVpnSettingWidget()
{
    delete d;
}

void OpenVpnSettingWidget::loadConfig(const NetworkManager::Setting::Ptr &setting)
{
    Q_UNUSED(setting)

    // General settings
    const NMStringMap dataMap = d->setting->data();
    const QString cType = dataMap.value(NM_OPENVPN_KEY_CONNECTION_TYPE);

    if (cType == QLatin1String(NM_OPENVPN_CONTYPE_PASSWORD_TLS)) {
        d->ui.cmbConnectionType->setCurrentIndex(Private::EnumConnectionType::CertsPassword);
        d->ui.x509PassUsername->setText(dataMap[NM_OPENVPN_KEY_USERNAME]);
        d->ui.x509PassCaFile->setUrl(QUrl::fromLocalFile(dataMap[NM_OPENVPN_KEY_CA]));
        d->ui.x509PassCert->setUrl(QUrl::fromLocalFile(dataMap[NM_OPENVPN_KEY_CERT]));
        d->ui.x509PassKey->setUrl(QUrl::fromLocalFile(dataMap[NM_OPENVPN_KEY_KEY]));
    } else if (cType == QLatin1String( NM_OPENVPN_CONTYPE_STATIC_KEY)) {
        d->ui.cmbConnectionType->setCurrentIndex( Private::EnumConnectionType::Psk );
        d->ui.pskSharedKey->setText( dataMap[NM_OPENVPN_KEY_STATIC_KEY]);
        if (dataMap.contains(NM_OPENVPN_KEY_STATIC_KEY_DIRECTION)) {
            switch (dataMap[NM_OPENVPN_KEY_STATIC_KEY_DIRECTION].toUInt()) {
            case 0:
                d->ui.cmbKeyDirection->setCurrentIndex(Private::EnumKeyDirection::D0);
                break;
            case 1:
                d->ui.cmbKeyDirection->setCurrentIndex(Private::EnumKeyDirection::D1);
                break;
            }
        } else {
            d->ui.cmbKeyDirection->setCurrentIndex(Private::EnumKeyDirection::None);
        }
        d->ui.pskRemoteIp->setText(dataMap[NM_OPENVPN_KEY_REMOTE_IP]);
        d->ui.pskLocalIp->setText(dataMap[NM_OPENVPN_KEY_LOCAL_IP]);
    } else if (cType == QLatin1String(NM_OPENVPN_CONTYPE_PASSWORD)) {
        d->ui.cmbConnectionType->setCurrentIndex(Private::EnumConnectionType::Password);
        d->ui.passUserName->setText(dataMap[NM_OPENVPN_KEY_USERNAME] );
        d->ui.passCaFile->setUrl(QUrl::fromLocalFile(dataMap[NM_OPENVPN_KEY_CA]));
    } else if (cType == QLatin1String(NM_OPENVPN_CONTYPE_TLS)) {
        d->ui.cmbConnectionType->setCurrentIndex(Private::EnumConnectionType::Certificates);
        d->ui.x509CaFile->setUrl(QUrl::fromLocalFile(dataMap[NM_OPENVPN_KEY_CA]));
        d->ui.x509Cert->setUrl(QUrl::fromLocalFile(dataMap[NM_OPENVPN_KEY_CERT]));
        d->ui.x509Key->setUrl(QUrl::fromLocalFile(dataMap[NM_OPENVPN_KEY_KEY]));
    }

    d->ui.gateway->setText( dataMap[NM_OPENVPN_KEY_REMOTE] );

    NetworkManager::Setting::SecretFlags type;

    if (cType == QLatin1String(NM_OPENVPN_CONTYPE_TLS)) {
        type = (NetworkManager::Setting::SecretFlags)dataMap[NM_OPENVPN_KEY_CERTPASS"-flags"].toInt();
        fillOnePasswordCombo(d->ui.x509KeyPassword, type);
    } else if (cType == QLatin1String(NM_OPENVPN_CONTYPE_PASSWORD)) {
        type = (NetworkManager::Setting::SecretFlags)dataMap[NM_OPENVPN_KEY_PASSWORD"-flags"].toInt();
        fillOnePasswordCombo(d->ui.passPassword, type);
    } else if (cType == QLatin1String(NM_OPENVPN_CONTYPE_PASSWORD_TLS)) {
        type = (NetworkManager::Setting::SecretFlags)dataMap[NM_OPENVPN_KEY_PASSWORD"-flags"].toInt();
        fillOnePasswordCombo(d->ui.x509PassPassword, type);
        type = (NetworkManager::Setting::SecretFlags)dataMap[NM_OPENVPN_KEY_CERTPASS"-flags"].toInt();
        fillOnePasswordCombo(d->ui.x509PassKeyPassword, type);
    }

    loadSecrets(setting);
}

void OpenVpnSettingWidget::loadSecrets(const NetworkManager::Setting::Ptr &setting)
{
    NetworkManager::VpnSetting::Ptr vpnSetting = setting.staticCast<NetworkManager::VpnSetting>();

    if (vpnSetting) {
        const QString cType = d->setting->data().value(NM_OPENVPN_KEY_CONNECTION_TYPE);
        const NMStringMap secrets = vpnSetting->secrets();

        if (cType == QLatin1String(NM_OPENVPN_CONTYPE_TLS)) {
            d->ui.x509KeyPassword->setText(secrets.value(NM_OPENVPN_KEY_CERTPASS));
        } else if (cType == QLatin1String(NM_OPENVPN_CONTYPE_PASSWORD)) {
            d->ui.passPassword->setText(secrets.value(NM_OPENVPN_KEY_PASSWORD));
        } else if (cType == QLatin1String(NM_OPENVPN_CONTYPE_PASSWORD_TLS)) {
            d->ui.x509PassPassword->setText(secrets.value(NM_OPENVPN_KEY_PASSWORD));
            d->ui.x509PassKeyPassword->setText(secrets.value(NM_OPENVPN_KEY_CERTPASS));
        }
    }
}

QVariantMap OpenVpnSettingWidget::setting() const
{
    NMStringMap data = d->setting->data();
    NMStringMap secretData = d->setting->secrets();
    NetworkManager::VpnSetting setting;
    setting.setServiceType(QLatin1String(NM_DBUS_SERVICE_OPENVPN));
    // required settings
    data.insert(QLatin1String(NM_OPENVPN_KEY_REMOTE), d->ui.gateway->text());

    QString contype;

    switch ( d->ui.cmbConnectionType->currentIndex()) {
    case Private::EnumConnectionType::Certificates:
        contype = QLatin1String(NM_OPENVPN_CONTYPE_TLS);
        // qCDebug(PLASMA_NM) << "saving VPN TLS settings as urls:" << d->ui.x509CaFile->url() << d->ui.x509Cert->url() << d->ui.x509Key->url();
        data.insert(QLatin1String(NM_OPENVPN_KEY_CA), d->ui.x509CaFile->url().toLocalFile());
        data.insert(QLatin1String(NM_OPENVPN_KEY_CERT), d->ui.x509Cert->url().toLocalFile());
        data.insert(QLatin1String(NM_OPENVPN_KEY_KEY), d->ui.x509Key->url().toLocalFile());
        // key password
        if (!d->ui.x509KeyPassword->text().isEmpty()) {
            secretData.insert(QLatin1String(NM_OPENVPN_KEY_CERTPASS), d->ui.x509KeyPassword->text());
        }
        handleOnePasswordType(d->ui.x509KeyPassword, QLatin1String(NM_OPENVPN_KEY_CERTPASS"-flags"), data);
        break;
    case Private::EnumConnectionType::Psk:
        contype = QLatin1String(NM_OPENVPN_CONTYPE_STATIC_KEY);
        data.insert(QLatin1String(NM_OPENVPN_KEY_STATIC_KEY), d->ui.pskSharedKey->url().toLocalFile());
        switch (d->ui.cmbKeyDirection->currentIndex()) {
        case Private::EnumKeyDirection::None:
            break;
        case Private::EnumKeyDirection::D0:
            data.insert(QLatin1String(NM_OPENVPN_KEY_STATIC_KEY_DIRECTION), QString::number(0));
            break;
        case Private::EnumKeyDirection::D1:
            data.insert(QLatin1String(NM_OPENVPN_KEY_STATIC_KEY_DIRECTION), QString::number(1));
            break;
        }
        // ip addresses
        data.insert(QLatin1String(NM_OPENVPN_KEY_REMOTE_IP), d->ui.pskRemoteIp->text());
        data.insert(QLatin1String(NM_OPENVPN_KEY_LOCAL_IP), d->ui.pskLocalIp->text());
        break;
    case Private::EnumConnectionType::Password:
        contype = QLatin1String(NM_OPENVPN_CONTYPE_PASSWORD);
        // username
        if (!d->ui.passUserName->text().isEmpty()) {
            data.insert(QLatin1String(NM_OPENVPN_KEY_USERNAME), d->ui.passUserName->text());
        }
        // password
        if (!d->ui.passPassword->text().isEmpty()) {
            secretData.insert(QLatin1String(NM_OPENVPN_KEY_PASSWORD ), d->ui.passPassword->text());
        }
        handleOnePasswordType(d->ui.passPassword, QLatin1String(NM_OPENVPN_KEY_PASSWORD"-flags"), data);
        // ca
        data.insert(QLatin1String(NM_OPENVPN_KEY_CA), d->ui.passCaFile->url().toLocalFile());
        break;
    case Private::EnumConnectionType::CertsPassword:
        contype = QLatin1String(NM_OPENVPN_CONTYPE_PASSWORD_TLS);
        // username
        if (!d->ui.x509PassUsername->text().isEmpty()) {
            data.insert(QLatin1String(NM_OPENVPN_KEY_USERNAME), d->ui.x509PassUsername->text());
        }
        // ca
        data.insert(QLatin1String(NM_OPENVPN_KEY_CA), d->ui.x509PassCaFile->url().toLocalFile());
        // cert
        data.insert(QLatin1String(NM_OPENVPN_KEY_CERT), d->ui.x509PassCert->url().toLocalFile());
        // key file
        data.insert(QLatin1String(NM_OPENVPN_KEY_KEY), d->ui.x509PassKey->url().toLocalFile());
        // key password
        if (!d->ui.x509PassKeyPassword->text().isEmpty()) {
            secretData.insert(QLatin1String(NM_OPENVPN_KEY_CERTPASS), d->ui.x509PassKeyPassword->text());
        }
        handleOnePasswordType(d->ui.x509PassKeyPassword, QLatin1String(NM_OPENVPN_KEY_CERTPASS"-flags"), data);
        // password
        if (!d->ui.x509PassPassword->text().isEmpty()) {
            secretData.insert(QLatin1String(NM_OPENVPN_KEY_PASSWORD), d->ui.x509PassPassword->text());
        }
        handleOnePasswordType(d->ui.x509PassPassword, QLatin1String(NM_OPENVPN_KEY_PASSWORD"-flags"), data);
        break;
    }
    data.insert(QLatin1String(NM_OPENVPN_KEY_CONNECTION_TYPE), contype);

    setting.setData(data);
    setting.setSecrets(secretData);

    return setting.toMap();
}

void OpenVpnSettingWidget::updateStartDir(const QUrl &url)
{
    QList<KUrlRequester *> requesters;
    requesters << d->ui.x509CaFile << d->ui.x509Cert << d->ui.x509Key << d->ui.pskSharedKey << d->ui.passCaFile << d->ui.x509PassCaFile
               << d->ui.x509PassCert << d->ui.x509PassKey;
    Q_FOREACH (KUrlRequester * requester, requesters) {
        requester->setStartDir(url.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash));
    }
}

void OpenVpnSettingWidget::setPasswordType(QLineEdit *edit, int type)
{
    edit->setEnabled(type == SettingWidget::EnumPasswordStorageType::Store);
}

void OpenVpnSettingWidget::fillOnePasswordCombo(PasswordField *passwordField, NetworkManager::Setting::SecretFlags type)
{
    if (type.testFlag(NetworkManager::Setting::None)) {
        passwordField->setPasswordOption(PasswordField::StoreForAllUsers);
    } else if (type.testFlag(NetworkManager::Setting::AgentOwned)) {
        passwordField->setPasswordOption(PasswordField::StoreForUser);
    } else if (type.testFlag(NetworkManager::Setting::NotSaved)) {
        passwordField->setPasswordOption(PasswordField::AlwaysAsk);
    } else {
        passwordField->setPasswordOption(PasswordField::PasswordField::NotRequired);
    }
}

void OpenVpnSettingWidget::handleOnePasswordType(const PasswordField *passwordField, const QString &key, NMStringMap &data) const
{
    const PasswordField::PasswordOption option = passwordField->passwordOption();
    switch (option) {
    case PasswordField::StoreForAllUsers:
        data.insert(key, QString::number(NetworkManager::Setting::None));
        break;
    case PasswordField::StoreForUser:
        data.insert(key, QString::number(NetworkManager::Setting::AgentOwned));
        break;
    case PasswordField::AlwaysAsk:
        data.insert(key, QString::number(NetworkManager::Setting::NotSaved));
        break;
    case PasswordField::NotRequired:
        data.insert(key, QString::number(NetworkManager::Setting::NotRequired));
        break;
    }
}

void OpenVpnSettingWidget::showAdvanced()
{
    QPointer<OpenVpnAdvancedWidget> adv = new OpenVpnAdvancedWidget(d->setting, this);
    adv->init();
    connect(adv.data(), &OpenVpnAdvancedWidget::accepted,
            [adv, this] () {
                NetworkManager::VpnSetting::Ptr advData = adv->setting();
                if (!advData.isNull()) {
                    d->setting->setData(advData->data());
                    d->setting->setSecrets(advData->secrets());
                }
            });
    connect(adv.data(), &OpenVpnAdvancedWidget::finished,
            [adv] () {
                if (adv) {
                    adv->deleteLater();
                }
            });
    adv->setModal(true);
    adv->show();
}

bool OpenVpnSettingWidget::isValid() const
{
    return !d->ui.gateway->text().isEmpty();
}
