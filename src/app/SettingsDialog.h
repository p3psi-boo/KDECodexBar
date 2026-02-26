#pragma once

#include <KConfigDialog>

class QComboBox;
class QCheckBox;

class SettingsDialog : public KConfigDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    // Getters for current settings
    int refreshInterval() const; // in ms, -1 for manual
    bool isAutostartEnabled() const;

    bool isCodexEnabled() const;
    bool isClaudeEnabled() const;
    bool isGeminiEnabled() const;
    bool isAntigravityEnabled() const;
    bool isAmpEnabled() const;

signals:
    void settingsChanged();

private slots:
    void updateAutostart(bool enable);

protected slots:
    void updateSettings() override;
    void updateWidgets() override;
    void updateWidgetsDefault() override;

protected:
    bool hasChanged() override;
    bool isDefault() override;

private:
    int readRefreshInterval() const;
    bool readAutostartEnabled() const;

    bool readCodexEnabled() const;
    bool readClaudeEnabled() const;
    bool readGeminiEnabled() const;
    bool readAntigravityEnabled() const;
    bool readAmpEnabled() const;

    QComboBox *m_intervalCombo;
    QCheckBox *m_autostartCheck;

    QCheckBox *m_codexCheck;
    QCheckBox *m_claudeCheck;
    QCheckBox *m_geminiCheck;
    QCheckBox *m_antigravityCheck;
    QCheckBox *m_ampCheck;
};
