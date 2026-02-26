#include "SettingsDialog.h"
#include "AppConfigKeys.h"
#include <KLocalizedString>
#include <KSharedConfig>
#include <KConfigGroup>
#include <KCoreConfigSkeleton>
#include <QVBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QFormLayout>
#include <QLayout>
#include <QStyle>
#include <QDir>
#include <QStandardPaths>
#include <QTextStream>
#include <QFileInfo>
#include <QCoreApplication>

namespace {

KSharedConfig::Ptr openSharedConfig() {
    return KSharedConfig::openConfig(QString::fromLatin1(AppConfigKeys::kConfigFile));
}

KConfigGroup generalGroup() {
    return KConfigGroup(openSharedConfig(), QString::fromLatin1(AppConfigKeys::kGeneralGroup));
}

KConfigGroup platformsGroup() {
    return KConfigGroup(openSharedConfig(), QString::fromLatin1(AppConfigKeys::kPlatformsGroup));
}

class SettingsSkeleton final : public KCoreConfigSkeleton {
public:
    SettingsSkeleton()
        : KCoreConfigSkeleton(openSharedConfig()) {
        setCurrentGroup(QString::fromLatin1(AppConfigKeys::kGeneralGroup));
        addItemInt(QStringLiteral("RefreshInterval"), m_refreshInterval, AppConfigKeys::kDefaultRefreshIntervalMs, QString::fromLatin1(AppConfigKeys::kRefreshIntervalKey));
        addItemBool(QStringLiteral("Autostart"), m_autostart, AppConfigKeys::kDefaultAutostart, QString::fromLatin1(AppConfigKeys::kAutostartKey));
        load();
    }

private:
    qint32 m_refreshInterval = AppConfigKeys::kDefaultRefreshIntervalMs;
    bool m_autostart = AppConfigKeys::kDefaultAutostart;
};

}

SettingsDialog::SettingsDialog(QWidget *parent)
    : KConfigDialog(parent, QStringLiteral("kdecodexbar-settings"), new SettingsSkeleton())
{
    setWindowTitle(i18n("Settings"));
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setFaceType(KPageDialog::Plain);

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

    auto *platformsBox = new QGroupBox(i18n("Platform statistics"), page);
    auto *platformsLayout = new QVBoxLayout(platformsBox);

    auto *platformsHint = new QLabel(i18n("Enable or disable usage tracking per platform."), platformsBox);
    platformsHint->setWordWrap(true);
    platformsLayout->addWidget(platformsHint);

    m_codexCheck = new QCheckBox(i18n("Codex"), platformsBox);
    m_claudeCheck = new QCheckBox(i18n("Claude"), platformsBox);
    m_geminiCheck = new QCheckBox(i18n("Gemini"), platformsBox);
    m_antigravityCheck = new QCheckBox(i18n("Antigravity"), platformsBox);
    m_ampCheck = new QCheckBox(i18n("Amp"), platformsBox);

    platformsLayout->addWidget(m_codexCheck);
    platformsLayout->addWidget(m_claudeCheck);
    platformsLayout->addWidget(m_geminiCheck);
    platformsLayout->addWidget(m_antigravityCheck);
    platformsLayout->addWidget(m_ampCheck);
    layout->addWidget(platformsBox);

    layout->addStretch();

    addPage(page, i18n("General"), QStringLiteral("settings-configure"), QString(), false);

    connect(m_intervalCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        updateButtons();
    });
    connect(m_autostartCheck, &QCheckBox::toggled, this, [this](bool) {
        updateButtons();
    });

    auto onPlatformToggled = [this](bool) {
        updateButtons();
    };
    connect(m_codexCheck, &QCheckBox::toggled, this, onPlatformToggled);
    connect(m_claudeCheck, &QCheckBox::toggled, this, onPlatformToggled);
    connect(m_geminiCheck, &QCheckBox::toggled, this, onPlatformToggled);
    connect(m_antigravityCheck, &QCheckBox::toggled, this, onPlatformToggled);
    connect(m_ampCheck, &QCheckBox::toggled, this, onPlatformToggled);

    updateWidgets();
}

int SettingsDialog::refreshInterval() const {
    return m_intervalCombo->currentData().toInt();
}

bool SettingsDialog::isAutostartEnabled() const {
    return m_autostartCheck->isChecked();
}

bool SettingsDialog::isCodexEnabled() const { return m_codexCheck->isChecked(); }
bool SettingsDialog::isClaudeEnabled() const { return m_claudeCheck->isChecked(); }
bool SettingsDialog::isGeminiEnabled() const { return m_geminiCheck->isChecked(); }
bool SettingsDialog::isAntigravityEnabled() const { return m_antigravityCheck->isChecked(); }
bool SettingsDialog::isAmpEnabled() const { return m_ampCheck->isChecked(); }

void SettingsDialog::updateWidgets() {
    const int interval = readRefreshInterval();
    int index = m_intervalCombo->findData(interval);
    if (index != -1) {
        m_intervalCombo->setCurrentIndex(index);
    } else {
        m_intervalCombo->setCurrentIndex(1);
    }

    m_autostartCheck->setChecked(readAutostartEnabled());

    m_codexCheck->setChecked(readCodexEnabled());
    m_claudeCheck->setChecked(readClaudeEnabled());
    m_geminiCheck->setChecked(readGeminiEnabled());
    m_antigravityCheck->setChecked(readAntigravityEnabled());
    m_ampCheck->setChecked(readAmpEnabled());
    updateButtons();
}

void SettingsDialog::updateWidgetsDefault() {
    int index = m_intervalCombo->findData(AppConfigKeys::kDefaultRefreshIntervalMs);
    if (index == -1) {
        index = 1;
    }
    m_intervalCombo->setCurrentIndex(index);

    m_autostartCheck->setChecked(AppConfigKeys::kDefaultAutostart);

    m_codexCheck->setChecked(AppConfigKeys::kDefaultPlatformEnabled);
    m_claudeCheck->setChecked(AppConfigKeys::kDefaultPlatformEnabled);
    m_geminiCheck->setChecked(AppConfigKeys::kDefaultPlatformEnabled);
    m_antigravityCheck->setChecked(AppConfigKeys::kDefaultPlatformEnabled);
    m_ampCheck->setChecked(AppConfigKeys::kDefaultPlatformEnabled);
    updateButtons();
}

void SettingsDialog::updateSettings() {
    // Use a single KConfig instance for all groups, so Apply/OK flushes a coherent view.
    KSharedConfig::Ptr cfg = openSharedConfig();

    KConfigGroup general(cfg, QString::fromLatin1(AppConfigKeys::kGeneralGroup));
    general.writeEntry(QString::fromLatin1(AppConfigKeys::kRefreshIntervalKey), refreshInterval());
    general.writeEntry(QString::fromLatin1(AppConfigKeys::kAutostartKey), isAutostartEnabled());

    KConfigGroup platforms(cfg, QString::fromLatin1(AppConfigKeys::kPlatformsGroup));
    platforms.writeEntry(QString::fromLatin1(AppConfigKeys::kEnableCodexKey), isCodexEnabled());
    platforms.writeEntry(QString::fromLatin1(AppConfigKeys::kEnableClaudeKey), isClaudeEnabled());
    platforms.writeEntry(QString::fromLatin1(AppConfigKeys::kEnableGeminiKey), isGeminiEnabled());
    platforms.writeEntry(QString::fromLatin1(AppConfigKeys::kEnableAntigravityKey), isAntigravityEnabled());
    platforms.writeEntry(QString::fromLatin1(AppConfigKeys::kEnableAmpKey), isAmpEnabled());

    // Ensure the file is actually written.
    cfg->sync();

    updateAutostart(isAutostartEnabled());

    emit settingsChanged();
    updateButtons();
}

bool SettingsDialog::hasChanged() {
    return refreshInterval() != readRefreshInterval()
        || isAutostartEnabled() != readAutostartEnabled()
        || isCodexEnabled() != readCodexEnabled()
        || isClaudeEnabled() != readClaudeEnabled()
        || isGeminiEnabled() != readGeminiEnabled()
        || isAntigravityEnabled() != readAntigravityEnabled()
        || isAmpEnabled() != readAmpEnabled();
}

bool SettingsDialog::isDefault() {
    return refreshInterval() == AppConfigKeys::kDefaultRefreshIntervalMs
        && isAutostartEnabled() == AppConfigKeys::kDefaultAutostart
        && isCodexEnabled() == AppConfigKeys::kDefaultPlatformEnabled
        && isClaudeEnabled() == AppConfigKeys::kDefaultPlatformEnabled
        && isGeminiEnabled() == AppConfigKeys::kDefaultPlatformEnabled
        && isAntigravityEnabled() == AppConfigKeys::kDefaultPlatformEnabled
        && isAmpEnabled() == AppConfigKeys::kDefaultPlatformEnabled;
}

int SettingsDialog::readRefreshInterval() const {
    KConfigGroup group = generalGroup();
    return group.readEntry(QString::fromLatin1(AppConfigKeys::kRefreshIntervalKey), AppConfigKeys::kDefaultRefreshIntervalMs);
}

bool SettingsDialog::readAutostartEnabled() const {
    KConfigGroup group = generalGroup();
    return group.readEntry(QString::fromLatin1(AppConfigKeys::kAutostartKey), AppConfigKeys::kDefaultAutostart);
}

bool SettingsDialog::readCodexEnabled() const {
    KConfigGroup group = platformsGroup();
    return group.readEntry(QString::fromLatin1(AppConfigKeys::kEnableCodexKey), AppConfigKeys::kDefaultPlatformEnabled);
}

bool SettingsDialog::readClaudeEnabled() const {
    KConfigGroup group = platformsGroup();
    return group.readEntry(QString::fromLatin1(AppConfigKeys::kEnableClaudeKey), AppConfigKeys::kDefaultPlatformEnabled);
}

bool SettingsDialog::readGeminiEnabled() const {
    KConfigGroup group = platformsGroup();
    return group.readEntry(QString::fromLatin1(AppConfigKeys::kEnableGeminiKey), AppConfigKeys::kDefaultPlatformEnabled);
}

bool SettingsDialog::readAntigravityEnabled() const {
    KConfigGroup group = platformsGroup();
    return group.readEntry(QString::fromLatin1(AppConfigKeys::kEnableAntigravityKey), AppConfigKeys::kDefaultPlatformEnabled);
}

bool SettingsDialog::readAmpEnabled() const {
    KConfigGroup group = platformsGroup();
    return group.readEntry(QString::fromLatin1(AppConfigKeys::kEnableAmpKey), AppConfigKeys::kDefaultPlatformEnabled);
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
