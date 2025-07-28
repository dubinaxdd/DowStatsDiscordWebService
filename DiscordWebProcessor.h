#ifndef DISCORDWEBPROCESSOR_H
#define DISCORDWEBPROCESSOR_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QWebSocket>
#include <QTimer>
#include "BaseTypes.h"
#include "defines.h"

enum MessageType: int
{
    NewsMessage = Qt::UserRole,
    EventMessage,
    TestMessage,
    Unknown
};

struct Message{
    QString id = "";
    QString timestamp = "";
    QString content = "";
    QString userName = "";
    QString userId = "";
    QString avatarId = "";
    QString avatarUrl = "";
    QString attacmentImageId = "";
    QString attacmentImageUrl = "";
    int attacmentImageWidth = 0;
    int attacmentImageHeight = 0;
    MessageType messageType = Unknown;
};

class DiscorsWebProcessor : public QObject
{
    Q_OBJECT
public:
    explicit DiscorsWebProcessor(QObject *parent = nullptr);

    enum OpCode: int{
        DefaultMessage = 0,
        HertbeatMessge = 1,
        IdentifyMessage = 2,
        ResumeMessage = 6,
        ReconnectMessage = 7,
        InvalidSessionMessage = 9,
        HelloMessage = 10,
        HertbeatAnsverMessage = 11,
    };

    void sendIdentify();
    void sendResume();

    QList<Message *>* newsMessages();
    QList<Message *>* eventMessages();
    QList<Message *>* testMessages();

signals:
    void sendEvent(QString messageId, EventType eventType);

private slots:
    void readDiscordWebSocket(QString messgae);
    void sendHeartbeat();
    void onDisconnected();
    void reconnect();
    void requestNextMessagesGroup();

private:
    void requestCnannelMessages(QString channelId, QString lastMessageId);
    void requestCnannelMessage(QString channelId, QString messageId, EventType eventType);
    void receiveMessages(QNetworkReply* reply, EventType eventType);
    Message* parseMessage(QJsonObject* message);
    void removeMessage(QString messageId, QList<Message*>* messagesList);

private:
    QNetworkAccessManager* m_networkManager;
    QWebSocket m_discordWebSocket;
    bool m_discordWebSocketConnected = false;
    QString lastMessageS = "null";
    QTimer m_heartbeatTimer;
    QString m_reconnectAdress = "";
    QString m_sessionId = "";
    bool m_isPlannedReconnect = false;
    bool m_isFirstConnect = true;

    QList<Message*> m_newsMessages;
    QList<Message*> m_eventMessages;
    QList<Message*> m_testMessages;

    QString m_newsLastMessageId = "";
    QString m_eventsLastMessageId = "";
    QString m_testLastMessageId = "";

    QTimer m_reconnectTimer;
    QTimer requestNextMessageGroupTimer;
    QStringList m_messageGroupChannelId = {TEST_CHANNEL_ID, EVENTS_CHANNEL_ID, NEWS_CHANNEL_ID};
    bool m_readyToNextRequest = true;

};

#endif // DISCORDWEBPROCESSOR_H
