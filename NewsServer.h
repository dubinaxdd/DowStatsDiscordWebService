#ifndef NEWSSERVER_H
#define NEWSSERVER_H

#include <QObject>
#include <QWebSocketServer>
#include <QWebSocket>
#include "DiscordWebProcessor.h"
#include "BaseTypes.h"

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
        RequestNewsFromIdByLimit = 4,
        RequestEventsFromIdByLimit = 5
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

private:
    void sendLastMessagesId(QWebSocket* client);
    Message* findMessage(QString messageId, QList<Message*>* messagesListPtr);

private:
    QWebSocketServer* m_server;
    QList<QWebSocket*> m_clientsList;
    QList<Message*> *p_newsMessages;
    QList<Message*> *p_eventsMessages;
    QList<Message*> *p_testMessages;
};

#endif // NEWSSERVER_H
