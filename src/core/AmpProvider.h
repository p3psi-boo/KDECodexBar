#pragma once

#include "Provider.h"
#include "PtySession.h"

class AmpProvider : public Provider {
    Q_OBJECT
public:
    explicit AmpProvider(QObject *parent = nullptr);
    ~AmpProvider() override = default;

    void refresh() override;

private slots:
    void onPtyData(const QByteArray &data);
    void onProcessExited(int exitCode);

private:
    void parseOutput(const QString &output);
    void cleanup();

    PtySession *m_session = nullptr;
    bool m_fetching = false;
    QString m_buffer;
};
