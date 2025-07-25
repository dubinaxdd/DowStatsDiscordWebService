#ifndef DISCORDWEBPROCESSOR_H
#define DISCORDWEBPROCESSOR_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QWebSocket>
#include <QTimer>

struct Message{
    QString id = "";
    QString timestamp = "";
    QString content = "";
    QString userName = "";
    QString userId = "";
    QString avatarId = "";
    QString avatarUrl = "";
    QString attacmentImageUrl = "";
};

class DiscorsWebProcessor : public QObject
{
    Q_OBJECT
public:
    explicit DiscorsWebProcessor(QObject *parent = nullptr);
    void sendIdentify();
    void sendResume();

signals:

private slots:
    void readDiscordWebSocket(QString messgae);
    void sendHeartbeat();
    void onDisconnected();
    void reconnect();

private:
    void requestCnannelMessages(QString channelId);
    void requestCnannelMessage(QString channelId, QString messageId);
    void receiveMessages(QNetworkReply* reply);
    Message* parseMessage(QJsonObject* message);

private:
    QNetworkAccessManager* m_networkManager;
    bool m_readyToRequest = true;
    QWebSocket m_discordWebSocket;
    bool m_discordWebSocketConnected = false;
    QString lastMessageS = "null";
    QTimer m_heartbeatTimer;
    QString m_reconnectAdress = "";
    QString m_sessionId = "";
    bool m_isPlannedReconnect = false;
    bool m_isFirstConnect = true;

    QList<Message*> m_messages;

    QTimer m_reconnectTimer;

};

#endif // DISCORDWEBPROCESSOR_H
