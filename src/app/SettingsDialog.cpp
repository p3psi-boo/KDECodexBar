#include "SettingsDialog.h"
#include <KLocalizedString>
#include <KSharedConfig>
#include <KConfigGroup>
#include <KCoreConfigSkeleton>
#include <QVBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QFormLayout>
#include <QLayout>
#include <QStyle>
#include <QDir>
#include <QDialogButtonBox>
#include <QStandardPaths>
#include <QTextStream>
#include <QFileInfo>
#include <QCoreApplication>

namespace {

constexpr auto kConfigFile = "kdecodexbarrc";
constexpr auto kConfigGroup = "General";
constexpr auto kRefreshKey = "refresh_interval";
constexpr auto kAutostartKey = "autostart";
constexpr int kDefaultRefreshInterval = 60000;
constexpr bool kDefaultAutostart = false;

KConfigGroup settingsGroup() {
    return KConfigGroup(KSharedConfig::openConfig(QString::fromLatin1(kConfigFile)), QString::fromLatin1(kConfigGroup));
}

class SettingsSkeleton final : public KCoreConfigSkeleton {
public:
    SettingsSkeleton()
        : KCoreConfigSkeleton(KSharedConfig::openConfig(QString::fromLatin1(kConfigFile))) {
        setCurrentGroup(QString::fromLatin1(kConfigGroup));
        addItemInt(QStringLiteral("RefreshInterval"), m_refreshInterval, kDefaultRefreshInterval, QString::fromLatin1(kRefreshKey));
        addItemBool(QStringLiteral("Autostart"), m_autostart, kDefaultAutostart, QString::fromLatin1(kAutostartKey));
        load();
    }

private:
    qint32 m_refreshInterval = kDefaultRefreshInterval;
    bool m_autostart = kDefaultAutostart;
};

}

SettingsDialog::SettingsDialog(QWidget *parent)
    : KConfigDialog(parent, QStringLiteral("kdecodexbar-settings"), new SettingsSkeleton())
{
    setWindowTitle(i18n("Settings"));
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setFaceType(KPageDialog::Plain);
    setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults);

    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);

    const int margin = style()->pixelMetric(QStyle::PM_LayoutLeftMargin, nullptr, page);
    const int verticalSpacing = style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing, nullptr, page);
    const int horizontalSpacing = style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing, nullptr, page);
    layout->setContentsMargins(margin, margin, margin, margin);
    layout->setSpacing(verticalSpacing > 0 ? verticalSpacing : 8);

    QFormLayout *formLayout = new QFormLayout();
    formLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    formLayout->setHorizontalSpacing(horizontalSpacing > 0 ? horizontalSpacing : 12);
    formLayout->setVerticalSpacing(verticalSpacing > 0 ? verticalSpacing : 8);

    m_intervalCombo = new QComboBox(page);
    m_intervalCombo->addItem(i18n("Manual"), -1);
    m_intervalCombo->addItem(i18n("1 minute"), 60000);
    m_intervalCombo->addItem(i18n("3 minutes"), 180000);
    m_intervalCombo->addItem(i18n("5 minutes"), 300000);
    m_intervalCombo->addItem(i18n("15 minutes"), 900000);
    formLayout->addRow(i18n("Refresh interval:"), m_intervalCombo);
    layout->addLayout(formLayout);

    m_autostartCheck = new QCheckBox(i18n("Run at startup"), page);
    layout->addWidget(m_autostartCheck);

    layout->addStretch();

    addPage(page, i18n("General"), QStringLiteral("settings-configure"), QString(), false);

    connect(m_intervalCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        updateButtons();
    });
    connect(m_autostartCheck, &QCheckBox::toggled, this, [this](bool) {
        updateButtons();
    });

    updateWidgets();
}

int SettingsDialog::refreshInterval() const {
    return m_intervalCombo->currentData().toInt();
}

bool SettingsDialog::isAutostartEnabled() const {
    return m_autostartCheck->isChecked();
}

void SettingsDialog::updateWidgets() {
    const int interval = readRefreshInterval();
    int index = m_intervalCombo->findData(interval);
    if (index != -1) {
        m_intervalCombo->setCurrentIndex(index);
    } else {
        m_intervalCombo->setCurrentIndex(1);
    }

    m_autostartCheck->setChecked(readAutostartEnabled());
    updateButtons();
}

void SettingsDialog::updateWidgetsDefault() {
    int index = m_intervalCombo->findData(kDefaultRefreshInterval);
    if (index == -1) {
        index = 1;
    }
    m_intervalCombo->setCurrentIndex(index);
    m_autostartCheck->setChecked(kDefaultAutostart);
    updateButtons();
}

void SettingsDialog::updateSettings() {
    KConfigGroup group = settingsGroup();
    group.writeEntry(kRefreshKey, refreshInterval());
    group.writeEntry(kAutostartKey, isAutostartEnabled());
    group.sync();

    updateAutostart(isAutostartEnabled());

    emit settingsChanged();
    updateButtons();
}

bool SettingsDialog::hasChanged() {
    return refreshInterval() != readRefreshInterval()
        || isAutostartEnabled() != readAutostartEnabled();
}

bool SettingsDialog::isDefault() {
    return refreshInterval() == kDefaultRefreshInterval
        && isAutostartEnabled() == kDefaultAutostart;
}

int SettingsDialog::readRefreshInterval() const {
    KConfigGroup group = settingsGroup();
    return group.readEntry(kRefreshKey, kDefaultRefreshInterval);
}

bool SettingsDialog::readAutostartEnabled() const {
    KConfigGroup group = settingsGroup();
    return group.readEntry(kAutostartKey, kDefaultAutostart);
}

void SettingsDialog::updateAutostart(bool enable) {
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    QString autostartDir = configDir + "/autostart";
    QDir dir(autostartDir);
    if (!dir.exists()) dir.mkpath(".");

    QString desktopFile = autostartDir + "/kdecodexbar.desktop";

    if (enable) {
        QFile file(desktopFile);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << "[Desktop Entry]\n";
            out << "Type=Application\n";
            out << "Name=KDECodexBar\n";
            out << "Exec=" << QCoreApplication::applicationFilePath() << "\n";
            out << "Icon=kdecodexbar\n";
            out << "Comment=AI Usage Tracker\n";
            out << "X-GNOME-Autostart-enabled=true\n";
            out << "Terminal=false\n";
            file.close();
        }
    } else {
        QFile::remove(desktopFile);
    }
}
