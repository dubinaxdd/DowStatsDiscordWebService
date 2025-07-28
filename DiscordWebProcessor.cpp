#include "DiscordWebProcessor.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include "QJsonDocument"
#include "QJsonArray"
#include "QJsonObject"


DiscorsWebProcessor::DiscorsWebProcessor(QObject *parent)
    : QObject{parent}
    , m_networkManager(new QNetworkAccessManager(this))
{
    qRegisterMetaType<EventType>("EventType");

    connect(&requestNextMessageGroupTimer, &QTimer::timeout, this,  &DiscorsWebProcessor::requestNextMessagesGroup, Qt::QueuedConnection);
    requestNextMessageGroupTimer.setInterval(2000);
    requestNextMessageGroupTimer.start();

    requestNextMessagesGroup();

    connect(&m_discordWebSocket, &QWebSocket::connected, this, [&]{qDebug() << "DiscorWebSocket connected"; m_discordWebSocketConnected = true; m_reconnectTimer.stop();}, Qt::QueuedConnection);
    connect(&m_discordWebSocket, &QWebSocket::disconnected, this,  &DiscorsWebProcessor::onDisconnected, Qt::QueuedConnection);
    connect(&m_discordWebSocket, &QWebSocket::textMessageReceived, this, &DiscorsWebProcessor::readDiscordWebSocket, Qt::QueuedConnection);

    connect(&m_heartbeatTimer, &QTimer::timeout, this, &DiscorsWebProcessor::sendHeartbeat, Qt::QueuedConnection);

    m_discordWebSocket.open(QUrl("wss://gateway.discord.gg/?v=10&encoding=json"));

    connect(&m_reconnectTimer, &QTimer::timeout, this, &DiscorsWebProcessor::reconnect, Qt::QueuedConnection);

    m_reconnectTimer.setInterval(5000);
    m_reconnectTimer.start();

    //m_discordWebSocket.open(QUrl("wss://gateway.discord.gg/"));

}

void DiscorsWebProcessor::sendIdentify()
{
    QJsonObject propertiesObject;
    propertiesObject.insert("os", "windows");
    propertiesObject.insert("browser", "DowStats");
    propertiesObject.insert("device", "DowStats");

    QJsonObject dObject;
    dObject.insert("token", DISCORD_TOKEN);
    dObject.insert("properties", propertiesObject);
    dObject.insert("compress", false);
    dObject.insert("intents", 1 << 9);

    QJsonObject identifyObject;
    identifyObject.insert("op", IdentifyMessage);
    identifyObject.insert("d", dObject);

    QJsonDocument jsonDocument;
    jsonDocument.setObject(identifyObject);

    m_discordWebSocket.sendTextMessage(jsonDocument.toJson().replace("\n",""));
    qDebug() << "OUTPUT DISCORD: IDENTIFY";
}

void DiscorsWebProcessor::sendResume()
{
    if (!m_discordWebSocketConnected)
        return;

    qDebug() << "OTPUT DISCORD: RESUME" << lastMessageS << m_sessionId;

    QJsonObject dObject;
    dObject.insert("token", /*"ASDASFDSDFFGNBDFGHJDSRTJHSDFGNSRTJ"*/DISCORD_TOKEN);
    dObject.insert("session_id", m_sessionId);
    dObject.insert("seq", lastMessageS.toInt());

    QJsonObject resumeObject;
    resumeObject.insert("op", ResumeMessage);
    resumeObject.insert("d", dObject);

    QJsonDocument jsonDocument;
    jsonDocument.setObject(resumeObject);

    m_discordWebSocket.sendTextMessage(jsonDocument.toJson().replace("\n",""));
}

void DiscorsWebProcessor::readDiscordWebSocket(QString messgae)
{
    QJsonDocument jsonDocument = QJsonDocument::fromJson(messgae.toUtf8());

    if (!jsonDocument.isObject())
        return;

    QJsonObject obeject = jsonDocument.object();

    int opcode = obeject.value("op").toInt();

    if (!obeject.value("s").isNull())
        lastMessageS = QString::number(obeject.value("s").toInt());

    QString messageType = obeject.value("t").toString();

    switch (opcode){
        case HelloMessage:
        {
            QJsonObject helloObject = obeject.value("d").toObject();
            int heartbeatInterval = helloObject.value("heartbeat_interval").toInt();
            m_heartbeatTimer.setInterval(heartbeatInterval / 2);
            m_heartbeatTimer.start();

            qDebug() << "INPUT DISCORD: HELLO" << lastMessageS;


            if (!m_isPlannedReconnect && m_isFirstConnect)
            {
                sendIdentify();
                m_isFirstConnect = false;
            }
            else
                sendResume();

            m_isPlannedReconnect = false;

            break;
        }

        case DefaultMessage:{
            if(messageType == "READY")
            {
                QJsonObject readyObject = obeject.value("d").toObject();
                m_reconnectAdress = readyObject.value("resume_gateway_url").toString();
                m_sessionId = readyObject.value("session_id").toString();
                qDebug() << "INPUT DISCORD: READY" << lastMessageS << "Reconnect address:" << m_reconnectAdress << "SessionId:" << m_sessionId;
            }

            if(messageType == "MESSAGE_CREATE")
            {
                QJsonObject messageCreate = obeject.value("d").toObject();
                QString channelId = messageCreate.value("channel_id").toString();
                QString messageId = messageCreate.value("id").toString();
                qDebug() << "INPUT DISCORD: MESSAGE_CREATE" << channelId << messageId;

                if(channelId == NEWS_CHANNEL_ID)
                    requestCnannelMessage(channelId, messageId, CreateNewsMessage);
                else if(channelId == EVENTS_CHANNEL_ID)
                    requestCnannelMessage(channelId, messageId, CreateEventMessage);
                else if(channelId == TEST_CHANNEL_ID)
                    requestCnannelMessage(channelId, messageId, CreateTestMessage);
            }

            if(messageType == "MESSAGE_UPDATE")
            {
                QJsonObject messageUpdate = obeject.value("d").toObject();
                QString channelId = messageUpdate.value("channel_id").toString();
                QString messageId = messageUpdate.value("id").toString();
                qDebug() << "INPUT DISCORD: MESSAGE_UPDATE" << channelId << messageId;

                if(channelId == NEWS_CHANNEL_ID)
                    requestCnannelMessage(channelId, messageId, UpdateNewsMessage);
                else if(channelId == EVENTS_CHANNEL_ID)
                    requestCnannelMessage(channelId, messageId, UpdateEventMessage);
                else if(channelId == TEST_CHANNEL_ID)
                    requestCnannelMessage(channelId, messageId, UpdateTestMessage);
            }

            break;
        }

        case HertbeatAnsverMessage:{
            qDebug() << "INPUT DISCORD: HEARTBEAT ANSWER" << lastMessageS;
            break;
        }

        case ReconnectMessage:{
            qDebug() << "INPUT DISCORD: RECONNECT" << lastMessageS;
            m_isPlannedReconnect = true;
            m_discordWebSocket.close();
            m_discordWebSocket.open(QUrl(m_reconnectAdress));
            break;
        }

        case InvalidSessionMessage:{
            qDebug() << "INPUT DISCORD: Invalid Session" << lastMessageS;
            sendIdentify();
            break;
        }

        default: qDebug() << "INPUT DISCORD: Unknown message" << messgae << lastMessageS; break;
    }
}

void DiscorsWebProcessor::requestCnannelMessages(QString channelId)
{
    QNetworkRequest newRequest;

    QString urlString = "https://discord.com/api/v10/channels/" + channelId + "/messages";

    //QString urlString = "https://discord.com/api/v10/channels/" + channelId;

    newRequest.setUrl(QUrl(urlString));
    newRequest.setRawHeader("Authorization", QString(DISCORD_TOKEN).toLocal8Bit());
    newRequest.setRawHeader("Host", "discord.com");
    newRequest.setRawHeader("User-Agent", "DowStatsClient");
    newRequest.setRawHeader("Content-Type","application/json");

    QNetworkReply *reply = m_networkManager->get(newRequest);

    QObject::connect(reply, &QNetworkReply::finished, this, [=](){
        receiveMessages(reply, AllMesagesReceive);
    });
}

void DiscorsWebProcessor::requestCnannelMessage(QString channelId, QString messageId, EventType eventType)
{
    QNetworkRequest newRequest;

    QString urlString = "https://discord.com/api/v10/channels/" + channelId + "/messages/" + messageId;

    newRequest.setUrl(QUrl(urlString));
    newRequest.setRawHeader("Authorization", QString(DISCORD_TOKEN).toLocal8Bit());
    newRequest.setRawHeader("Host", "discord.com");
    newRequest.setRawHeader("User-Agent", "DowStatsClient");
    newRequest.setRawHeader("Content-Type","application/json");

    QNetworkReply *reply = m_networkManager->get(newRequest);

    QObject::connect(reply, &QNetworkReply::finished, this, [=](){
        receiveMessages(reply, eventType);
    });
}

void DiscorsWebProcessor::receiveMessages(QNetworkReply *reply, EventType eventType)
{
    if (reply->error() != QNetworkReply::NoError)
    {
        reply->deleteLater();
        return;
    }

    QByteArray replyByteArray = reply->readAll();
    QJsonDocument jsonDocument = QJsonDocument::fromJson(replyByteArray);

    if (jsonDocument.isArray())
    {
        for (auto item : jsonDocument.array())
        {
            QJsonObject object = item.toObject();

            auto message = parseMessage(&object);

            switch(message->messageType)
            {
                case NewsMessage: m_newsMessages.append(message); break;
                case EventMessage: m_eventMessages.append(message); break;
                case TestMessage: m_testMessages.append(message); break;
                case Unknown: break;
            }

            //qDebug() << m_messages.last()->id << m_messages.last()->timestamp << m_messages.last()->userId << m_messages.last()->userName << m_messages.last()->avatarId << m_messages.last()->avatarUrl << m_messages.last()->content << m_messages.last()->attacmentImageUrl;
        }
    }
    else if (jsonDocument.isObject())
    {
        QJsonObject object = jsonDocument.object();
        auto message = parseMessage(&object);

        QList<Message*>* temp;

        switch(message->messageType)
        {
        case NewsMessage: temp = &m_newsMessages; break;
        case EventMessage: temp = &m_eventMessages; break;
        case TestMessage: temp = &m_testMessages; break;
        case Unknown: break;
        }

        bool finded = false;

        for (auto& item : *temp) {
            if(item->id == message->id)
            {
                delete item;
                item = message;
                finded = true;
                break;
            }
        }

        if (!finded)
            temp->append(message);

        emit sendEvent(message->id, eventType);

        //qDebug() << m_messages.last()->id << m_messages.last()->timestamp << m_messages.last()->userId << m_messages.last()->userName << m_messages.last()->avatarId << m_messages.last()->avatarUrl << m_messages.last()->content << m_messages.last()->attacmentImageUrl;
    }
}

Message *DiscorsWebProcessor::parseMessage(QJsonObject *message)
{
    Message* newMessage = new Message();

    QString channelId = message->value("channel_id").toString();

    newMessage->id = message->value("id").toString();

    if (channelId == NEWS_CHANNEL_ID)
        newMessage->messageType = NewsMessage;

    if (channelId == EVENTS_CHANNEL_ID)
        newMessage->messageType = EventMessage;

    if (channelId == TEST_CHANNEL_ID)
        newMessage->messageType = TestMessage;

    newMessage->content = message->value("content").toString();
    newMessage->timestamp = message->value("timestamp").toString();

    QJsonObject authorObject = message->value("author").toObject();
    newMessage->userId = authorObject.value("id").toString();
    newMessage->userName = authorObject.value("global_name").toString();
    newMessage->avatarId = authorObject.value("avatar").toString();
    newMessage->avatarUrl = "https://cdn.discordapp.com/avatars/" + newMessage->userId + "/" + newMessage->avatarId + ".png";

    QJsonArray attachmentArray =  message->value("attachments").toArray();

    if (!attachmentArray.isEmpty())
    {
        for (auto item : attachmentArray)
        {
            QJsonObject attachmentObject = item.toObject();
            QString contentType = attachmentObject.value("content_type").toString();

            if (contentType == "image/png" || contentType == "image/jpeg")
            {
                newMessage->attacmentImageUrl = attachmentObject.value("url").toString();
                break;
            }
        }
    }

    if (newMessage->attacmentImageUrl.isEmpty())
    {
        QJsonArray embedsArray =  message->value("embeds").toArray();

        if (!embedsArray.isEmpty())
        {
            for (auto item : embedsArray)
            {
                QJsonObject embedsObject = item.toObject();
                QString contentType = embedsObject.value("type").toString();

                if (contentType == "image")
                {
                    newMessage->attacmentImageUrl = embedsObject.value("url").toString();
                    break;
                }
            }
        }
    }

    newMessage->content.replace(newMessage->attacmentImageUrl, "");

    return newMessage;
}

void DiscorsWebProcessor::requestNextMessagesGroup()
{
    requestCnannelMessages(m_messageGroupChannelId.last());
    m_messageGroupChannelId.removeLast();

    if(m_messageGroupChannelId.isEmpty())
        requestNextMessageGroupTimer.stop();
}

QList<Message *>* DiscorsWebProcessor::newsMessages()
{
    return &m_newsMessages;
}

QList<Message *>* DiscorsWebProcessor::eventMessages()
{
    return &m_eventMessages;
}

QList<Message *> *DiscorsWebProcessor::testMessages()
{
    return &m_testMessages;
}

void DiscorsWebProcessor::sendHeartbeat()
{
    if (!m_discordWebSocketConnected)
        return;

    qDebug() << "OTPUT DISCORD: HEARTBEAT" << lastMessageS;
    m_discordWebSocket.sendTextMessage("{\"op\": " + QString::number(HertbeatMessge)+ ",\"d\":" + lastMessageS +"}");
}

void DiscorsWebProcessor::onDisconnected()
{
    qDebug() << "DiscorWebSocket Disconnected";
    m_discordWebSocketConnected = false;

    if (m_isPlannedReconnect)
        return;

    m_discordWebSocket.close();
    m_reconnectTimer.start();
}

void DiscorsWebProcessor::reconnect()
{
    if (m_discordWebSocketConnected)
        return;

    qDebug() << "Reconnect" << m_reconnectAdress;
    m_discordWebSocket.open(QUrl(m_reconnectAdress));
}
