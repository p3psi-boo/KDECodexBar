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

    QComboBox *m_intervalCombo;
    QCheckBox *m_autostartCheck;
};
