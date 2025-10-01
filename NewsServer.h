#ifndef NEWSSERVER_H
#define NEWSSERVER_H

#include <QObject>
#include <QWebSocketServer>
#include <QWebSocket>
#include "DiscordWebProcessor.h"
#include "BaseTypes.h"
#include <QTimer>

struct Client{
    QWebSocket* webSocket;
    quint32 pingTime = 0;
};

class NewsServer : public QObject
{
    Q_OBJECT
public:
    explicit NewsServer(QObject *parent = nullptr);
    ~NewsServer();

    enum OpCode : int
    {
        Autentication = 0,
        RequestLastMessagesId = 1,
        RequestNewsFromIdToId = 2,
        RequestEventsFromIdToId = 3,
        RequestTestFromIdToId = 4,
        RequestNewsFromIdByLimit = 5,
        RequestEventsFromIdByLimit = 6,
        RequestTestFromIdByLimit = 7,
        Ping = 8
    };

    void setNewsMessagesListPtr(QList<Message *> *newsMeessagesList);
    void setEventsMessagesListPtr(QList<Message *> *eventsMeessagesList);
    void setTestMessagesListPtr(QList<Message *> *testMeessagesList);

public slots:
    void onNewConnection();
    void onClientDisconnectd();
    void onMessageReceived(const QString &message);
    void onClosed();
    void onEventReceived(QString messageId, EventType eventType);

    void onDataReady();

    void checkClientsPingTime();

private:
    void sendLastMessagesId(QWebSocket* client);
    void sendMessagesFromIdByLimit(QWebSocket* client, EventType eventType, QString messageId, int limit, bool includeFirst, QList<Message*>* messagesListPtr);
    void sendPingResponce(QWebSocket* client);
    QJsonObject messageToJson(Message*);

    Message* findMessage(QString messageId, QList<Message*>* messagesListPtr);

private:
    QWebSocketServer* m_server;
    QList<Client> m_clientsList;
    QList<Message*> *p_newsMessages;
    QList<Message*> *p_eventsMessages;
    QList<Message*> *p_testMessages;
    QTimer m_checkTimeTimer;
};

#endif // NEWSSERVER_H
