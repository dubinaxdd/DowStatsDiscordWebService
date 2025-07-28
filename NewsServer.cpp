#include "NewsServer.h"

#include "QJsonDocument"
#include "QJsonArray"
#include "QJsonObject"

NewsServer::NewsServer(QObject *parent)
    : QObject{parent}
    , m_server(new QWebSocketServer("DowStatsNewsService", QWebSocketServer::NonSecureMode, this))
{
    m_server->listen(QHostAddress::Any, 50789);
    connect(m_server, &QWebSocketServer::newConnection, this, &NewsServer::onNewConnection);
    connect(m_server, &QWebSocketServer::closed, this, &NewsServer::onClosed);
}

NewsServer::~NewsServer()
{
    m_server->close();
}

void NewsServer::onNewConnection()
{
    QWebSocket *newSocket = m_server->nextPendingConnection();

    connect(newSocket, &QWebSocket::textMessageReceived, this, &NewsServer::onMessageReceived);
    connect(newSocket, &QWebSocket::disconnected, this, &NewsServer::onClientDisconnectd);

    m_clientsList.append(newSocket);
}

void NewsServer::onClientDisconnectd()
{
    QWebSocket *disconnectedClient = qobject_cast<QWebSocket *>(sender());

    if (disconnectedClient) {
        m_clientsList.removeAll(disconnectedClient);
        disconnectedClient->deleteLater();
    }
}

void NewsServer::onMessageReceived(const QString &message)
{
    QWebSocket *client = qobject_cast<QWebSocket *>(sender());

    QJsonDocument jsonDocument = QJsonDocument::fromJson(message.toUtf8());

    if (!jsonDocument.isObject())
        return;

    QJsonObject jsonObject = jsonDocument.object();

    int opCode = jsonObject.value("op").toInt();

    switch (opCode){
    case RequestLastMessagesId: sendLastMessagesId(client); break;
    }
}

void NewsServer::onClosed()
{
    for (auto& item : m_clientsList)
    {
        if (item)
            item->disconnect();
    }
}

void NewsServer::onEventReceived(QString messageId, EventType eventType)
{
    QJsonObject messageObject;
    messageObject.insert("op", eventType);

    Message* message = nullptr;

    switch (eventType){
        case EventType::CreateNewsMessage:
        case EventType::UpdateNewsMessage: message = findMessage(messageId, p_newsMessages); break;
        case EventType::CreateEventMessage:
        case EventType::UpdateEventMessage: message = findMessage(messageId, p_eventsMessages); break;
        case EventType::CreateTestMessage:
        case EventType::UpdateTestMessage: message = findMessage(messageId, p_testMessages); break;
        default: break;
    }

    QJsonObject contentObject;
    contentObject.insert("id", message->id);
    contentObject.insert("timestamp", message->timestamp);
    contentObject.insert("content", message->content);
    contentObject.insert("userName", message->userName);
    contentObject.insert("userId", message->userId);
    contentObject.insert("avatarId", message->avatarId);
    contentObject.insert("avatarUrl", message->avatarUrl);
    contentObject.insert("attacmentImageUrl", message->attacmentImageUrl);

    messageObject.insert("content", contentObject);

    QJsonDocument document;
    document.setObject(messageObject);

    QString text = document.toJson();

    std::for_each( std::begin(m_clientsList), std::end(m_clientsList),
        [&](QWebSocket* item){
            item->sendTextMessage(text);
        });
}

void NewsServer::sendLastMessagesId(QWebSocket *client)
{
    QString newsLastMessageId = "";
    QString eventsLastMessageId = "";
    QString testLastMessageId = "";

    if (!p_newsMessages->isEmpty())
        newsLastMessageId = p_newsMessages->first()->id;

    if (!p_eventsMessages->isEmpty())
        eventsLastMessageId = p_eventsMessages->first()->id;

    if (!p_testMessages->isEmpty())
        testLastMessageId = p_testMessages->first()->id;


    QJsonObject messageObject;
    messageObject.insert("op", RequestLastMessagesId);

    messageObject.insert("news_last_message", newsLastMessageId);
    messageObject.insert("events_last_message", eventsLastMessageId);
    messageObject.insert("test_last_message", testLastMessageId);

    QJsonDocument message;
    message.setObject(messageObject);

    client->sendTextMessage(message.toJson().replace('\n',""));
}

Message *NewsServer::findMessage(QString messageId, QList<Message *> *messagesListPtr)
{
    for(auto& item : *messagesListPtr)
    {
        if (item->id == messageId)
            return item;
    }
}

void NewsServer::setNewsMessagesListPtr(QList<Message *> *newsMeessagesList)
{
    p_newsMessages = newsMeessagesList;
}

void NewsServer::setEventsMessagesListPtr(QList<Message *> *eventsMeessagesList)
{
    p_eventsMessages = eventsMeessagesList;
}

void NewsServer::setTestMessagesListPtr(QList<Message *> *testMeessagesList)
{
    p_testMessages = testMeessagesList;
}
