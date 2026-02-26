#include "AmpProvider.h"
#include <QDebug>
#include <QRegularExpression>
#include <QStandardPaths>

AmpProvider::AmpProvider(QObject *parent)
    : Provider(ProviderID::Amp, parent)
    , m_session(nullptr)
    , m_fetching(false)
{
}

void AmpProvider::refresh() {
    if (m_fetching) return;

    QString ampPath = QStandardPaths::findExecutable("amp");
    if (ampPath.isEmpty()) {
        setState(ProviderState::Error);
        emit dataChanged();
        return;
    }

    m_fetching = true;
    m_buffer.clear();
    cleanup();

    m_session = new PtySession(this);
    connect(m_session, &PtySession::dataRead, this, &AmpProvider::onPtyData);
    connect(m_session, &PtySession::processExited, this, &AmpProvider::onProcessExited);

    if (!m_session->start("amp", {"usage"})) {
        setState(ProviderState::Error);
        cleanup();
        m_fetching = false;
        emit dataChanged();
    }
}

void AmpProvider::onPtyData(const QByteArray &data) {
    m_buffer.append(QString::fromUtf8(data));
    parseOutput(m_buffer);
}

void AmpProvider::onProcessExited(int exitCode) {
    Q_UNUSED(exitCode);
    cleanup();
    m_fetching = false;
}

void AmpProvider::cleanup() {
    if (m_session) {
        m_session->close();
        m_session->deleteLater();
        m_session = nullptr;
    }
}

void AmpProvider::parseOutput(const QString &output) {
    UsageSnapshot snap;
    snap.timestamp = QDateTime::currentDateTime();
    snap.limits.clear();

    static QRegularExpression ansiRegex(R"(\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~]))");
    QString clean = output;
    clean.remove(ansiRegex);

    static QRegularExpression ampFreeRegex(
        R"(Amp Free:\s*\$?([\d.]+)\s*/\s*\$?([\d.]+)\s*remaining)",
        QRegularExpression::CaseInsensitiveOption
    );
    auto match = ampFreeRegex.match(clean);
    if (match.hasMatch()) {
        double used = match.captured(1).toDouble();
        double total = match.captured(2).toDouble();
        
        UsageLimit limit;
        limit.label = "Amp Free";
        limit.used = used;
        limit.total = total;
        limit.unit = "$";
        snap.limits.append(limit);
    }

    static QRegularExpression bonusRegex(
        R"(\[\+?(\d+)%\s*bonus\s*for\s*(\d+)\s*days?\s*more\])",
        QRegularExpression::CaseInsensitiveOption
    );
    auto bonusMatch = bonusRegex.match(clean);
    if (bonusMatch.hasMatch()) {
        UsageLimit bonus;
        bonus.label = QString("+%1% bonus").arg(bonusMatch.captured(1));
        bonus.resetDescription = QString("%1 days left").arg(bonusMatch.captured(2));
        snap.limits.append(bonus);
    }

    static QRegularExpression creditsRegex(
        R"(Individual credits:\s*\$?([\d.]+)\s*remaining)",
        QRegularExpression::CaseInsensitiveOption
    );
    auto creditsMatch = creditsRegex.match(clean);
    if (creditsMatch.hasMatch()) {
        UsageLimit credits;
        credits.label = "Credits";
        credits.used = 0;
        credits.total = creditsMatch.captured(1).toDouble();
        credits.unit = "$";
        snap.limits.append(credits);
    }

    static QRegularExpression replenishRegex(
        R"(replenishes\s*\+?\$?([\d.]+)/hour)",
        QRegularExpression::CaseInsensitiveOption
    );
    auto replenishMatch = replenishRegex.match(clean);
    if (replenishMatch.hasMatch()) {
        for (auto &limit : snap.limits) {
            if (limit.label == "Amp Free") {
                limit.resetDescription = QString("+$%1/hour").arg(replenishMatch.captured(1));
                break;
            }
        }
    }

    if (!snap.limits.isEmpty()) {
        setSnapshot(snap);
        setState(ProviderState::Active);
        emit dataChanged();
    }
}
