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

struct Gateway {
    int maxConcurrency;
    int remaining;
    int resetAfter;
    int total;
    int shards;
    QString url;
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
    QString attacmentImageType = "";
    int attacmentImageWidth = 0;
    int attacmentImageHeight = 0;
    MessageType messageType = Unknown;
    bool needLoadAttachmentImage = false;
    bool needSendEvent = false;
    bool avatarReady = false;
    bool attachmentImageReady = false;
};

class DiscordWebProcessor : public QObject
{
    Q_OBJECT
public:
    explicit DiscordWebProcessor(QObject *parent = nullptr);

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
    void dataReady();

private slots:
    void readDiscordWebSocket(QString messgae);
    void sendHeartbeat();
    void onDisconnected();
    void reconnect();

    void receiveGateway(QNetworkReply* reply);

    void receiveUserAvatar(QNetworkReply* reply, Message *message, EventType eventType);
    void receiveAttachmentImage(QNetworkReply* reply, Message *message, EventType eventType);

    void requestNextMessagesGroup();

private:
    void requestGateway();
    void requestCnannelMessages(QString channelId, QString lastMessageId);
    void requestCnannelMessage(QString channelId, QString messageId, EventType eventType);
    void receiveMessages(QNetworkReply* reply, EventType eventType);
    Message* parseMessage(QJsonObject* message);
    void removeMessage(QString messageId, QList<Message*>* messagesList);
    void connectDiscordWebsocket();

    void requestAttachmentImage(Message* message, EventType eventType);
    void requestUserAvatar(Message* message, EventType eventType);

    QString getAttachmentImagePath(Message* message);
    QString getAttachmentImageUrl(Message* message);

    QString getAavatarImagePath(Message* message);
    QString getAvatarImageUrl(Message* message);

    void sendEventMessage(Message* message, EventType eventType);

    void updateAvatar(QList<Message *> *messagesList, Message* message);

    bool checkAvatarExist(Message* message);
    bool checkAttachmentImageExist(Message* message);

    void requestNextAttachmentImage();
    void requestNextAvatarImage();


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
    QStringList m_messageGroupChannelId = {"IMAGES", "AVATARS", TEST_CHANNEL_ID, EVENTS_CHANNEL_ID, NEWS_CHANNEL_ID};
    bool m_readyToNextRequest = true;

    Gateway m_gateway;

    QList<Message*> m_messagesForLoadAvatars;
    QList<Message*> m_messagesForLoadImages;
};

#endif // DISCORDWEBPROCESSOR_H
