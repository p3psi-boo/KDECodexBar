#include "TrayIcon.h"
#include "IconRenderer.h"
#include "AppConfigKeys.h"
#include <KLocalizedString>
#include <QAction>
#include <QIcon>
#include <QApplication>
#include <QCoreApplication>
#include <QCursor>
#include <QGuiApplication>
#include <QtGlobal>
#include "ProviderRegistry.h"
#include "Provider.h"
#include <KConfigGroup>
#include <KSharedConfig>

#if __has_include(<kstatusnotifieritem_version.h>)
#include <kstatusnotifieritem_version.h>
#endif

namespace {

KSharedConfig::Ptr sharedConfig() {
    auto cfg = KSharedConfig::openConfig(QString::fromLatin1(AppConfigKeys::kConfigFile));
    // KSharedConfig is cached per-thread; force reload so Settings Apply takes effect immediately.
    cfg->reparseConfiguration();
    return cfg;
}

KConfigGroup generalGroup() {
    return KConfigGroup(sharedConfig(), QString::fromLatin1(AppConfigKeys::kGeneralGroup));
}

KConfigGroup platformsGroup() {
    return KConfigGroup(sharedConfig(), QString::fromLatin1(AppConfigKeys::kPlatformsGroup));
}

bool isProviderEnabled(ProviderID id) {
    KConfigGroup group = platformsGroup();
    switch (id) {
    case ProviderID::Codex:
        return group.readEntry(QString::fromLatin1(AppConfigKeys::kEnableCodexKey), AppConfigKeys::kDefaultPlatformEnabled);
    case ProviderID::Claude:
        return group.readEntry(QString::fromLatin1(AppConfigKeys::kEnableClaudeKey), AppConfigKeys::kDefaultPlatformEnabled);
    case ProviderID::Gemini:
        return group.readEntry(QString::fromLatin1(AppConfigKeys::kEnableGeminiKey), AppConfigKeys::kDefaultPlatformEnabled);
    case ProviderID::Antigravity:
        return group.readEntry(QString::fromLatin1(AppConfigKeys::kEnableAntigravityKey), AppConfigKeys::kDefaultPlatformEnabled);
    case ProviderID::Amp:
        return group.readEntry(QString::fromLatin1(AppConfigKeys::kEnableAmpKey), AppConfigKeys::kDefaultPlatformEnabled);
    default:
        return false;
    }
}

ProviderID firstEnabledProviderID(ProviderRegistry *registry) {
    for (auto *provider : registry->providers()) {
        if (isProviderEnabled(provider->id())) {
            return provider->id();
        }
    }
    return ProviderID::Unknown;
}

ProviderID normalizeSelectedProvider(ProviderRegistry *registry, ProviderID current) {
    if (current != ProviderID::Unknown && registry->provider(current) && isProviderEnabled(current)) {
        return current;
    }
    return firstEnabledProviderID(registry);
}

QString formatUsageBar(double percent) {
    const int width = 14;
    const int filled = qBound(0, static_cast<int>((percent * width / 100.0) + 0.5), width);
    const QString full(filled, '#');
    const QString empty(width - filled, '-');
    return QString("[%1%2]").arg(full, empty);
}

}

TrayIcon::TrayIcon(ProviderRegistry *registry, QObject *parent)
    : QObject(parent)
    , m_sni(new KStatusNotifierItem(this))
    , m_menu(new QMenu())
    , m_registry(registry)
    , m_timer(new QTimer(this))
    , m_selectedProviderID(ProviderID::Codex)
{
    // Basic SNI setup
    m_sni->setCategory(KStatusNotifierItem::ApplicationStatus);
    m_sni->setStatus(KStatusNotifierItem::Active);
    m_sni->setStandardActionsEnabled(false); // We provide our own Quit action
    
    // Use custom rendered placeholder icon initially
    m_sni->setIconByPixmap(IconRenderer::renderPlaceholder());
    
    m_sni->setToolTip(
        "codexbar",
        i18n("CodexBar"),
        i18n("Tracking AI Provider Usage")
    );

    setupMenu();
    m_sni->setContextMenu(m_menu);

    // Prefer host-managed menu on single-click (fixes Wayland popup grabbing).
    // If KF is too old for setIsMenu(), keep a best-effort X11-only fallback.
#if defined(KSTATUSNOTIFIERITEM_VERSION) && (KSTATUSNOTIFIERITEM_VERSION >= QT_VERSION_CHECK(6, 14, 0))
    m_sni->setIsMenu(true);
#else
    if (QGuiApplication::platformName() == QStringLiteral("xcb")) {
        connect(m_sni, &KStatusNotifierItem::activateRequested, this,
                [this](bool active, const QPoint &pos) {
                    if (!active) return;
                    setupMenu();
                    const QPoint globalPos = pos.isNull() ? QCursor::pos() : pos;
                    m_menu->popup(globalPos);
                });
    }
#endif
    
    // Connect providers
    for (auto *provider : m_registry->providers()) {
        connect(provider, &Provider::dataChanged, this, &TrayIcon::updateIcon);
    }
    
    // Setup refresh timer (every 60s)
    connect(m_timer, &QTimer::timeout, this, [this](){
        for (auto *provider : m_registry->providers()) {
            if (isProviderEnabled(provider->id())) {
                provider->refresh();
            }
        }
    });
    m_timer->start(60000); 
    
    // Initial refresh
    QTimer::singleShot(0, this, [this](){
        m_selectedProviderID = normalizeSelectedProvider(m_registry, m_selectedProviderID);

        KConfigGroup group = generalGroup();
        int interval = group.readEntry(QString::fromLatin1(AppConfigKeys::kRefreshIntervalKey), AppConfigKeys::kDefaultRefreshIntervalMs);
        if (interval > 0) m_timer->start(interval);
        else m_timer->stop();

        for (auto *provider : m_registry->providers()) {
            if (isProviderEnabled(provider->id())) {
                provider->refresh();
            }
        }
    });
}

void TrayIcon::updateIcon() {
    m_selectedProviderID = normalizeSelectedProvider(m_registry, m_selectedProviderID);

    // 1. Icon Update
    // Only render icon for the selected provider
    auto *provider = m_registry->provider(m_selectedProviderID);
    bool iconUpdated = false;
    
    if (provider && provider->state() == ProviderState::Active) {
        auto snap = provider->snapshot();
        if (!snap.limits.isEmpty()) {
             m_sni->setIconByPixmap(IconRenderer::renderIcon(snap, false));
             iconUpdated = true;
        }
    }
    
    if (!iconUpdated) {
        m_sni->setIconByPixmap(IconRenderer::renderPlaceholder());
    }

    // 2. Tooltip Update
    // Only show selected provider in tooltip
    QString tooltip;
    
    if (provider && provider->state() == ProviderState::Active) {
         UsageSnapshot snap = provider->snapshot();
         if (!snap.limits.isEmpty()) {
            tooltip += QString("<b>%1</b>").arg(provider->name());
            for (const auto &limit : snap.limits) {
                  tooltip += QString("<br>&nbsp;&nbsp;%1: %2%")
                    .arg(limit.label)
                    .arg(limit.percent(), 0, 'f', 1);
            }
         }
    }

    // Use a descriptive title instead of App Name to ensure visibility and avoid duplication
    // User requested "CodexBar" as title -> "KDECodexBar"
    m_sni->setToolTip(QIcon(), "KDECodexBar", tooltip);
    
    // Refresh the menu actions (stats might have changed)
    setupMenu();
}

void TrayIcon::setupMenu() {
    m_menu->clear();

    // App Name
    auto *title = m_menu->addAction("KDECodexBar");
    title->setEnabled(false);
    m_menu->addSeparator();

    const ProviderID normalizedSelection = normalizeSelectedProvider(m_registry, m_selectedProviderID);
    m_selectedProviderID = normalizedSelection;

    bool anyPlatformEnabled = false;
    for (const auto &provider : m_registry->providers()) {
        if (!isProviderEnabled(provider->id())) {
            continue;
        }
        anyPlatformEnabled = true;

        // Section Header (Selectable)
        bool isSelected = (provider->id() == m_selectedProviderID);
        // Use text-based indicator to control spacing
        QString label = isSelected ? QStringLiteral("✓ %1").arg(provider->name()) 
                                   : QStringLiteral("   %1").arg(provider->name());
        
        QAction *header = m_menu->addAction(label);
        
        QFont font = header->font();
        font.setBold(true);
        header->setFont(font);
        
        // Handle selection
        connect(header, &QAction::triggered, this, [this, provider](){
            m_selectedProviderID = provider->id();
            updateIcon(); // Will redraw icon/tooltip and rebuild menu (updating checks)
        });

        UsageSnapshot snap = provider->snapshot();
        // Dynamic stats
        if (snap.limits.isEmpty()) {
             QAction *act = m_menu->addAction("     No usage data");
             act->setEnabled(false);
        } else {
             for (const auto &limit : snap.limits) {
                 const double pct = limit.percent();
                 const QString text = QString("     %1 %2 %3%")
                     .arg(limit.label.leftJustified(8, ' ', true))
                     .arg(formatUsageBar(pct))
                     .arg(QString::number(pct, 'f', 1));

                 QAction *chartLine = m_menu->addAction(text);
                 chartLine->setEnabled(false);

                 if (!limit.resetDescription.isEmpty()) {
                     QAction *resetLine = m_menu->addAction(QString("       %1").arg(limit.resetDescription));
                     resetLine->setEnabled(false);
                 }
             }
        }

        m_menu->addSeparator();
    }

    if (!anyPlatformEnabled) {
        QAction *act = m_menu->addAction(i18n("No platforms enabled"));
        act->setEnabled(false);
        m_menu->addSeparator();
    }
    
    // Settings & Refresh
    auto *settings = m_menu->addAction(i18n("Settings"));
    connect(settings, &QAction::triggered, this, [this](){
        if (!m_settingsDialog) {
            m_settingsDialog = new SettingsDialog(); // Create lazily or pass parent
            // Since TrayIcon is a QObject not QWidget, careful with parent logic for dialogs.
            // But we can just show it.
            connect(m_settingsDialog, &SettingsDialog::settingsChanged, this, &TrayIcon::applySettings);
        }
        m_settingsDialog->show();
        m_settingsDialog->raise();
        m_settingsDialog->activateWindow();
    });
    
    m_menu->addAction(i18n("Refresh All"), this, [this](){
        for (auto *provider : m_registry->providers()) {
            if (isProviderEnabled(provider->id())) {
                provider->refresh();
            }
        }
    });
    
    m_menu->addSeparator();

    // 4. Quit
    auto quitAction = m_menu->addAction(QIcon::fromTheme("application-exit"), i18n("Quit"));
    connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);
}

void TrayIcon::applySettings() {
    if (!m_settingsDialog) return;
    
    int interval = m_settingsDialog->refreshInterval();
    if (interval > 0) {
        m_timer->start(interval);
    } else {
        m_timer->stop();
    }

    // If selection got disabled, or platforms changed, update UI and refresh enabled ones.
    m_selectedProviderID = normalizeSelectedProvider(m_registry, m_selectedProviderID);
    updateIcon();
    for (auto *provider : m_registry->providers()) {
        if (isProviderEnabled(provider->id())) {
            provider->refresh();
        }
    }
}
