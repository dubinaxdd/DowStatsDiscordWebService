#include "NewsServer.h"

#include "QJsonDocument"
#include "QJsonArray"
#include "QJsonObject"

NewsServer::NewsServer(QObject *parent)
    : QObject{parent}
    , m_server(new QWebSocketServer("DowStatsNewsService", QWebSocketServer::NonSecureMode, this))
{
    connect(m_server, &QWebSocketServer::newConnection, this, &NewsServer::onNewConnection);
    connect(m_server, &QWebSocketServer::closed, this, &NewsServer::onClosed);
    connect(&m_checkTimeTimer, &QTimer::timeout, this, &NewsServer::checkClientsPingTime, Qt::QueuedConnection);

    m_checkTimeTimer.setInterval(1000);
}

NewsServer::~NewsServer()
{
    m_server->close();
}

void NewsServer::onNewConnection()
{
    Client newClient;
    newClient.webSocket = m_server->nextPendingConnection();

    connect(newClient.webSocket, &QWebSocket::textMessageReceived, this, &NewsServer::onMessageReceived);
    connect(newClient.webSocket, &QWebSocket::disconnected, this, &NewsServer::onClientDisconnectd);

    m_clientsList.append(std::move(newClient));
    qDebug() << "CLIENT CONNECTED";
}

void NewsServer::onClientDisconnectd()
{
    qDebug() << "ASDASDASDASDASD ON DISCONNECTED";
    QWebSocket *disconnectedClient = qobject_cast<QWebSocket *>(sender());

    if (disconnectedClient) {

        disconnectedClient->close();

        for (int i = 0; i < m_clientsList.count(); i++) {
            if(m_clientsList.at(i).webSocket == disconnectedClient)
            {
                m_clientsList.removeAt(i);
                qDebug() << "CLIENT DISCONNECTED";
                break;
            }
        }

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
    case RequestNewsFromIdByLimit:
        sendMessagesFromIdByLimit(client, NewsMessagesAnswer, jsonObject.value("messageId").toString(), jsonObject.value("limit").toInt(), jsonObject.value("includeFirst").toBool(), p_newsMessages);
        break;
    case RequestEventsFromIdByLimit:
        sendMessagesFromIdByLimit(client, EventsMessagesAnswer, jsonObject.value("messageId").toString(), jsonObject.value("limit").toInt(), jsonObject.value("includeFirst").toBool(), p_eventsMessages);
        break;
    case RequestTestFromIdByLimit:
        sendMessagesFromIdByLimit(client, TestMessagesAnswer, jsonObject.value("messageId").toString(), jsonObject.value("limit").toInt(), jsonObject.value("includeFirst").toBool(), p_testMessages);
        break;
    case Ping:{
        for (int i = 0; i < m_clientsList.count(); i++) {
            if(m_clientsList.at(i).webSocket == client)
            {
                m_clientsList[i].pingTime = 0;
                break;
            }
        }

        sendPingResponce(client);
        break;
    }
    }
}

void NewsServer::onClosed()
{
    for (auto& item : m_clientsList)
    {
        if (item.webSocket)
            item.webSocket->disconnect();
    }
}

void NewsServer::onEventReceived(QString messageId, EventType eventType)
{
    QJsonObject messageObject;
    messageObject.insert("op", eventType);

    Message* message = nullptr;

    switch (eventType){
        case EventType::CreateNewsMessage:
        case EventType::UpdateNewsMessage:
            message = findMessage(messageId, p_newsMessages);
            messageObject.insert("content", messageToJson(message));
            break;
        case EventType::DeleteNewsMessage:
            messageObject.insert("messageId", messageId);
            break;
        case EventType::CreateEventMessage:
        case EventType::UpdateEventMessage:
            message = findMessage(messageId, p_eventsMessages);
            messageObject.insert("content", messageToJson(message));
            break;
        case EventType::DeleteEventMessage:
            messageObject.insert("messageId", messageId);
            break;
        case EventType::CreateTestMessage:
        case EventType::UpdateTestMessage:
            message = findMessage(messageId, p_testMessages);
            messageObject.insert("content", messageToJson(message));
            break;
        case EventType::DeleteTestMessage:
            messageObject.insert("messageId", messageId);
            break;
        default: break;
    }

    QJsonDocument document;
    document.setObject(messageObject);

    QString text = document.toJson();


    for (auto& client : m_clientsList )
    {
        if (client.webSocket && client.webSocket->isValid() && client.webSocket->state() == QAbstractSocket::SocketState::ConnectedState)
            client.webSocket->sendTextMessage(text);
    }


    /*std::for_each( std::begin(m_clientsList), std::end(m_clientsList),
        [&](QWebSocket* item){
            item->sendTextMessage(text);
        });*/
}

void NewsServer::onDataReady()
{
    m_checkTimeTimer.start();
    m_server->listen(QHostAddress::Any, 50789);
    qDebug() << "Seervice started.";
}

void NewsServer::checkClientsPingTime()
{
    for(auto& item: m_clientsList)
    {
        item.pingTime++;

        if (item.pingTime > 45)
        {
            qDebug() << "PING TIMEOUT";
            if (item.webSocket)
                item.webSocket->close();
        }
    }
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
    messageObject.insert("op", LastMessagesIdAnswer);

    messageObject.insert("news_last_message", newsLastMessageId);
    messageObject.insert("events_last_message", eventsLastMessageId);
    messageObject.insert("test_last_message", testLastMessageId);

    QJsonDocument message;
    message.setObject(messageObject);

    client->sendTextMessage(message.toJson().replace('\n',""));
}

void NewsServer::sendMessagesFromIdByLimit(QWebSocket *client, EventType eventType, QString messageId, int limit, bool includeFirst, QList<Message*>* messagesListPtr)
{
    QJsonObject messageObject;
    QJsonArray messagesArray;

    for (int i = 0; i < messagesListPtr->count(); i++)
    {
        if (messagesListPtr->at(i)->id == messageId)
        {
            if(!includeFirst)
                i++;

            if (i >= messagesListPtr->count())
                return;

            for (int j = i; j < messagesListPtr->count(); j++)
            {
                messagesArray.append(messageToJson(messagesListPtr->at(j)));

                if (messagesArray.count() >= limit)
                    break;
            }

            break;
        }
    }

    if (messagesArray.isEmpty())
    {
        EventType endListReplyType;

        switch (eventType) {
            case NewsMessagesAnswer: endListReplyType = NewsMessagesEnd; break;
            case EventsMessagesAnswer: endListReplyType = EventsMessagesEnd; break;
            case TestMessagesAnswer: endListReplyType = TestMessagesEnd; break;
            default: break;
        }

        messageObject.insert("op", endListReplyType);

        QJsonDocument message;
        message.setObject(messageObject);

        client->sendTextMessage(message.toJson().replace('\n',""));
        return;
    }

    messageObject.insert("op", eventType);
    messageObject.insert("messages", messagesArray);

    QJsonDocument message;
    message.setObject(messageObject);

    client->sendTextMessage(message.toJson().replace('\n',""));
}

void NewsServer::sendPingResponce(QWebSocket *client)
{
    QJsonObject messageObject;
    messageObject.insert("op", PingResponce);

    QJsonDocument message;
    message.setObject(messageObject);

    client->sendTextMessage(message.toJson().replace('\n',""));

    qDebug() << "SEND PING RESPONCE";
}

QJsonObject NewsServer::messageToJson(Message * message)
{


    QJsonObject contentObject;

    if (!message)
        return contentObject;

    contentObject.insert("id", message->id);
    contentObject.insert("timestamp", message->timestamp);
    contentObject.insert("content", message->content);
    contentObject.insert("userName", message->userName);
    contentObject.insert("userId", message->userId);
    contentObject.insert("avatarId", message->avatarId);
    contentObject.insert("avatarUrl", message->avatarUrl);
    contentObject.insert("attacmentImageId", message->attacmentImageId);
    contentObject.insert("attacmentImageUrl", message->attacmentImageUrl);
    contentObject.insert("attacmentImageWidth", message->attacmentImageWidth);
    contentObject.insert("attacmentImageHeight", message->attacmentImageHeight);

    return contentObject;
}

Message *NewsServer::findMessage(QString messageId, QList<Message *> *messagesListPtr)
{
    for(auto& item : *messagesListPtr)
    {
        if (item->id == messageId)
            return item;
    }

    return nullptr;
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
