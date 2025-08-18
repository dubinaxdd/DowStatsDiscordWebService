#include "DiscordWebProcessor.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <QCoreApplication>

DiscordWebProcessor::DiscordWebProcessor(QObject *parent)
    : QObject{parent}
    , m_networkManager(new QNetworkAccessManager(this))
{
    qRegisterMetaType<EventType>("EventType");

    connect(&requestNextMessageGroupTimer, &QTimer::timeout, this,  &DiscordWebProcessor::requestNextMessagesGroup, Qt::QueuedConnection);
    requestNextMessageGroupTimer.setInterval(2000);

    requestGateway();

    connect(&m_discordWebSocket, &QWebSocket::connected, this, [&]{qDebug() << "DiscorWebSocket connected"; m_discordWebSocketConnected = true; m_reconnectTimer.stop();}, Qt::QueuedConnection);
    connect(&m_discordWebSocket, &QWebSocket::disconnected, this,  &DiscordWebProcessor::onDisconnected, Qt::QueuedConnection);
    connect(&m_discordWebSocket, &QWebSocket::textMessageReceived, this, &DiscordWebProcessor::readDiscordWebSocket, Qt::QueuedConnection);

    connect(&m_heartbeatTimer, &QTimer::timeout, this, &DiscordWebProcessor::sendHeartbeat, Qt::QueuedConnection);

    connect(&m_reconnectTimer, &QTimer::timeout, this, &DiscordWebProcessor::reconnect, Qt::QueuedConnection);
    m_reconnectTimer.setInterval(5000);
}

void DiscordWebProcessor::sendIdentify()
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

void DiscordWebProcessor::sendResume()
{
    if (!m_discordWebSocketConnected)
        return;

    qDebug() << "OTPUT DISCORD: RESUME" << lastMessageS << m_sessionId;

    QJsonObject dObject;
    dObject.insert("token", DISCORD_TOKEN);
    dObject.insert("session_id", m_sessionId);
    dObject.insert("seq", lastMessageS.toInt());

    QJsonObject resumeObject;
    resumeObject.insert("op", ResumeMessage);
    resumeObject.insert("d", dObject);

    QJsonDocument jsonDocument;
    jsonDocument.setObject(resumeObject);

    m_discordWebSocket.sendTextMessage(jsonDocument.toJson().replace("\n",""));
}

void DiscordWebProcessor::readDiscordWebSocket(QString messgae)
{
    QJsonDocument jsonDocument = QJsonDocument::fromJson(messgae.toLatin1());

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

            if(messageType == "MESSAGE_DELETE")
            {
                QJsonObject messageDelete = obeject.value("d").toObject();
                QString channelId = messageDelete.value("channel_id").toString();
                QString messageId = messageDelete.value("id").toString();
                qDebug() << "INPUT DISCORD: MESSAGE_UPDATE" << channelId << messageId;

                if(channelId == NEWS_CHANNEL_ID)
                {
                    removeMessage(messageId, &m_newsMessages);
                    emit sendEvent(messageId, DeleteNewsMessage);
                }
                else if(channelId == EVENTS_CHANNEL_ID)
                {
                    removeMessage(messageId, &m_eventMessages);
                    emit sendEvent(messageId, DeleteEventMessage);
                }
                else if(channelId == TEST_CHANNEL_ID)
                {
                    removeMessage(messageId, &m_testMessages);
                    emit sendEvent(messageId, DeleteTestMessage);
                }
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
            m_reconnectAdress = m_gateway.url;
            m_isFirstConnect = true;
            m_discordWebSocket.close();
            //connectDiscordWebsocket();
            //sendIdentify();
            break;
        }

        default: qDebug() << "INPUT DISCORD: Unknown message" << messgae << lastMessageS; break;
    }
}

void DiscordWebProcessor::requestCnannelMessages(QString channelId, QString lastMessageId)
{
    QNetworkRequest newRequest;

    QString urlString;

    if (lastMessageId.isEmpty())
        urlString = "https://discord.com/api/v10/channels/" + channelId + "/messages";
    else
        urlString = "https://discord.com/api/v10/channels/" + channelId + "/messages?before=" + lastMessageId;

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

void DiscordWebProcessor::requestCnannelMessage(QString channelId, QString messageId, EventType eventType)
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

void DiscordWebProcessor::receiveMessages(QNetworkReply *reply, EventType eventType)
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
        QJsonArray jsonArray = jsonDocument.array();

        for (auto item : jsonArray)
        {
            QJsonObject object = item.toObject();

            auto message = parseMessage(&object);

            switch(message->messageType)
            {
                case NewsMessage: m_newsMessages.append(message); m_newsLastMessageId = message->id; break;
                case EventMessage: m_eventMessages.append(message); m_eventsLastMessageId = message->id; break;
                case TestMessage: m_testMessages.append(message); m_testLastMessageId = message->id; break;
                case Unknown: break;
            }

            if (!checkAvatarExist(message))
                m_messagesForLoadAvatars.append(message);

            if (message->needLoadAttachmentImage && !checkAttachmentImageExist(message))
                m_messagesForLoadImages.append(message);

            //qDebug() << m_messages.last()->id << m_messages.last()->timestamp << m_messages.last()->userId << m_messages.last()->userName << m_messages.last()->avatarId << m_messages.last()->avatarUrl << m_messages.last()->content << m_messages.last()->attacmentImageUrl;
        }

        if (jsonArray.count() < 50)
            m_messageGroupChannelId.removeLast();

    }
    else if (jsonDocument.isObject())
    {
        QJsonObject object = jsonDocument.object();
        auto message = parseMessage(&object);

        message->needSendEvent = true;
        message->needUpdateAvatar = true;

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
            temp->push_front(message);

        if (!checkAvatarExist(message))
            requestUserAvatar(message, eventType);
        else
            message->avatarReady = true;

        if (!checkAttachmentImageExist(message) && message->needLoadAttachmentImage)
            requestAttachmentImage(message, eventType);
        else
            message->attachmentImageReady = true;

        sendEventMessage(message, eventType);

        //qDebug() << m_messages.last()->id << m_messages.last()->timestamp << m_messages.last()->userId << m_messages.last()->userName << m_messages.last()->avatarId << m_messages.last()->avatarUrl << m_messages.last()->content << m_messages.last()->attacmentImageUrl;
    }

    m_readyToNextRequest = true;

    reply->deleteLater();
}

Message *DiscordWebProcessor::parseMessage(QJsonObject *message)
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
                newMessage->attacmentImageId = attachmentObject.value("id").toString();
                newMessage->attacmentImageUrl = attachmentObject.value("url").toString();
                newMessage->attacmentImageWidth = attachmentObject.value("width").toInt();
                newMessage->attacmentImageHeight = attachmentObject.value("height").toInt();               
                newMessage->attacmentImageType = contentType.replace("image/", "");
                newMessage->needLoadAttachmentImage =true;

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
                    newMessage->attacmentImageId = embedsObject.value("url").toString();
                    newMessage->attacmentImageUrl = embedsObject.value("url").toString();

                    QJsonObject thumbnail = embedsObject.value("thumbnail").toObject();
                    newMessage->attacmentImageWidth = thumbnail.value("width").toInt();
                    newMessage->attacmentImageHeight = thumbnail.value("height").toInt();
                    break;
                }
            }
        }
    }

    newMessage->content.replace(newMessage->attacmentImageUrl, "");

    return newMessage;
}

void DiscordWebProcessor::removeMessage(QString messageId, QList<Message *> *messagesList)
{
    for(int i = 0; i < messagesList->count(); i++)
    {
        if (messagesList->at(i)->id == messageId)
        {
            delete messagesList->operator[](i);
            messagesList->removeAt(i);
            break;
        }
    }
}

void DiscordWebProcessor::connectDiscordWebsocket()
{
    // m_discordWebSocket.open(QUrl("wss://gateway.discord.gg/?v=10&encoding=json"));
    m_discordWebSocket.open(QUrl(m_gateway.url));
    m_isFirstConnect = true;
    m_reconnectTimer.start();
    requestNextMessageGroupTimer.start();
}

void DiscordWebProcessor::requestNextMessagesGroup()
{
    if(m_messageGroupChannelId.isEmpty())
    {
        requestNextMessageGroupTimer.stop();
        qDebug() << "All data received.";
        emit dataReady();
        return;
    }

    if (!m_readyToNextRequest)
        return;

    QString currentGroup = m_messageGroupChannelId.last();
    QString lastMessageId = "";



    if (currentGroup == "AVATARS" || currentGroup == "IMAGES")
    {
        qDebug() << "Request " << currentGroup << m_messagesForLoadAvatars.count() << m_messagesForLoadImages.count();

        if (currentGroup == "AVATARS")
        {
            requestNextMessageGroupTimer.setInterval(500);
            requestNextAvatarImage();
        }

        if (currentGroup == "IMAGES")
            requestNextAttachmentImage();
    }
    else
    {
        if (currentGroup == NEWS_CHANNEL_ID)
            lastMessageId = m_newsLastMessageId;

        if (currentGroup == EVENTS_CHANNEL_ID)
            lastMessageId = m_eventsLastMessageId;

        if (currentGroup == TEST_CHANNEL_ID)
            lastMessageId = m_testLastMessageId;


        m_readyToNextRequest = false;

        qDebug() << "Request Channel messages" << currentGroup << lastMessageId;
        requestCnannelMessages(currentGroup, lastMessageId);
    }
}

void DiscordWebProcessor::receiveGateway(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError)
    {
        reply->deleteLater();
        return;
    }

    QByteArray replyByteArray = reply->readAll();
    QJsonDocument jsonDocument = QJsonDocument::fromJson(replyByteArray);

    if (!jsonDocument.isObject())
        return;


    QJsonObject object = jsonDocument.object();

    QJsonObject sessionStartLimits = object.value("session_start_limit").toObject();
    m_gateway.maxConcurrency = sessionStartLimits.value("max_concurrency").toInt();
    m_gateway.remaining = sessionStartLimits.value("remaining").toInt();
    m_gateway.resetAfter = sessionStartLimits.value("reset_after").toInt();
    m_gateway.total = sessionStartLimits.value("total").toInt();
    m_gateway.shards = object.value("shards").toInt();
    m_gateway.url = object.value("url").toString();

    connectDiscordWebsocket();

    reply->deleteLater();
}

void DiscordWebProcessor::requestGateway()
{
    QNetworkRequest newRequest;

    QString urlString = "https://discord.com/api/v10/gateway/bot";


    newRequest.setUrl(QUrl(urlString));
    newRequest.setRawHeader("Authorization", QString(DISCORD_TOKEN).toLocal8Bit());
    newRequest.setRawHeader("Host", "discord.com");
    newRequest.setRawHeader("User-Agent", "DowStatsClient");
    newRequest.setRawHeader("Content-Type","application/json");

    QNetworkReply *reply = m_networkManager->get(newRequest);

    QObject::connect(reply, &QNetworkReply::finished, this, [=](){
        receiveGateway(reply);
    });
}

QList<Message *>* DiscordWebProcessor::newsMessages()
{
    return &m_newsMessages;
}

QList<Message *>* DiscordWebProcessor::eventMessages()
{
    return &m_eventMessages;
}

QList<Message *> *DiscordWebProcessor::testMessages()
{
    return &m_testMessages;
}

void DiscordWebProcessor::sendHeartbeat()
{
    if (!m_discordWebSocketConnected)
        return;

    qDebug() << "OTPUT DISCORD: HEARTBEAT" << lastMessageS;
    m_discordWebSocket.sendTextMessage("{\"op\": " + QString::number(HertbeatMessge)+ ",\"d\":" + lastMessageS +"}");
}

void DiscordWebProcessor::onDisconnected()
{
    qDebug() << "DiscorWebSocket Disconnected";
    m_discordWebSocketConnected = false;

    if (m_isPlannedReconnect)
        return;

    m_discordWebSocket.close();
    m_reconnectTimer.start();
}

void DiscordWebProcessor::reconnect()
{
    if (m_discordWebSocketConnected)
        return;

    qDebug() << "Reconnect" << m_reconnectAdress;
    m_discordWebSocket.open(QUrl(m_reconnectAdress));
}

void DiscordWebProcessor::requestAttachmentImage(Message* message, EventType eventType)
{
    m_readyToNextRequest = false;

    QNetworkRequest newRequest;

    newRequest.setUrl(QUrl(message->attacmentImageUrl));

    QNetworkReply *reply = m_networkManager->get(newRequest);

    QObject::connect(reply, &QNetworkReply::finished, this, [=](){
        receiveAttachmentImage(reply, message, eventType);
    });
}

void DiscordWebProcessor::receiveAttachmentImage(QNetworkReply *reply, Message* message, EventType eventType)
{
    if (reply->error() != QNetworkReply::NoError)
    {
        qDebug() << "DiscordWebProcessor::receiveAttachmentImage:" << "Connection error:" << reply->errorString();
        reply->deleteLater();
        message->attachmentImageReady = true;
        m_readyToNextRequest = true;
        sendEventMessage(message, eventType);
        return;
    }

    QByteArray replyByteArray = reply->readAll();

    reply->deleteLater();

    QFile imageFile(getAttachmentImagePath(message));

    if (imageFile.open(QIODevice::WriteOnly))
    {
        imageFile.write(replyByteArray);
        imageFile.close();
    }

    message->attacmentImageUrl = getAttachmentImageUrl(message);
    message->attachmentImageReady = true;
    m_readyToNextRequest = true;

    sendEventMessage(message, eventType);
}

void DiscordWebProcessor::requestUserAvatar(Message* message, EventType eventType)
{
    m_readyToNextRequest = false;

    QNetworkRequest newRequest;
    newRequest.setUrl(QUrl(message->avatarUrl));

    QNetworkReply *reply = m_networkManager->get(newRequest);

    QObject::connect(reply, &QNetworkReply::finished, this, [=](){
        receiveUserAvatar(reply, message, eventType);
    });
}


void DiscordWebProcessor::receiveUserAvatar(QNetworkReply *reply, Message* message, EventType eventType)
{
    if (reply->error() != QNetworkReply::NoError)
    {
        qDebug() << "DiscordWebProcessor::receiveUserAvatar:" << "Connection error:" << reply->errorString();
        reply->deleteLater();
        message->avatarReady = true;
        m_readyToNextRequest = true;
        sendEventMessage(message, eventType);
        return;
    }

    QByteArray replyByteArray = reply->readAll();

    reply->deleteLater();

    QFile imageFile(getAavatarImagePath(message));

    if (imageFile.open(QIODevice::WriteOnly))
    {
        imageFile.write(replyByteArray);
        imageFile.close();
    }

    message->avatarUrl = getAvatarImageUrl(message);
    message->avatarReady = true;

    if (message->needUpdateAvatar)
    {
        updateAvatar(&m_newsMessages, message);
        updateAvatar(&m_eventMessages, message);
        updateAvatar(&m_testMessages, message);
    }

    m_readyToNextRequest = true;
    sendEventMessage(message, eventType);
}

QString DiscordWebProcessor::getAttachmentImagePath(Message *message)
{
#ifdef WIN32
    QString dirPath = QCoreApplication::applicationDirPath() + QDir::separator() + "images";
#else
    QString dirPath = "/ftp/crosspick-ftp/discord_web_service/images";
#endif

    QDir dir;

    if (!dir.exists(dirPath)) {
        dir.mkdir(dirPath);
    }

    return dirPath + QDir::separator() + message->attacmentImageId + "." + message->attacmentImageType;
}

QString DiscordWebProcessor::getAttachmentImageUrl(Message *message)
{
    return "http://crosspick.ru/discord_web_service/images/" + message->attacmentImageId + "." + message->attacmentImageType;
}

QString DiscordWebProcessor::getAavatarImagePath(Message *message)
{
#ifdef WIN32
    QString dirPath = QCoreApplication::applicationDirPath() + QDir::separator() + "avatars";
#else
    QString dirPath = "/ftp/crosspick-ftp/discord_web_service/avatars";
#endif

   // qDebug() << QCoreApplication::applicationDirPath() + QDir::separator() + "avatars";
    //qDebug() << dirPath;

    QDir dir;

    if (!dir.exists(dirPath)) {
        dir.mkdir(dirPath);
    }

    return dirPath + QDir::separator() + message->avatarId + ".png";
}

QString DiscordWebProcessor::getAvatarImageUrl(Message *message)
{
    return "http://crosspick.ru/discord_web_service/avatars/" + message->avatarId + ".png";
}

void DiscordWebProcessor::sendEventMessage(Message *message, EventType eventType)
{
    if (message->needSendEvent && message->attachmentImageReady && message->avatarReady)
        emit sendEvent(message->id, eventType);
}

void DiscordWebProcessor::updateAvatar(QList<Message*>* messagesList, Message* message)
{
    for (auto& item : *messagesList)
    {
        if(item->userId == message->userId)
        {
            QFile imageFile(getAavatarImagePath(item));
            if(imageFile.exists())
                imageFile.remove();

            item->userName = message->userName;
            item->avatarId = message->avatarId;
            item->avatarUrl = message->avatarUrl;
        }
    }
}

bool DiscordWebProcessor::checkAvatarExist(Message *message)
{
    QFile imageFile(getAavatarImagePath(message));

    if (imageFile.exists())
    {
        message->avatarUrl = getAvatarImageUrl(message);
        return true;
    }

    return false;
}

bool DiscordWebProcessor::checkAttachmentImageExist(Message *message)
{
    QFile imageFile(getAttachmentImagePath(message));

    if (imageFile.exists())
    {
        message->attacmentImageUrl = getAttachmentImageUrl(message);
        return true;
    }

    return false;
}

void DiscordWebProcessor::requestNextAttachmentImage()
{
    if(m_messagesForLoadImages.isEmpty())
    {
        m_messageGroupChannelId.removeAll("IMAGES");
        requestNextMessagesGroup();
        return;
    }

    if (m_messagesForLoadImages.first()->needLoadAttachmentImage && !checkAttachmentImageExist(m_messagesForLoadImages.first()))
    {
        requestAttachmentImage(m_messagesForLoadImages.first(), AllMesagesReceive);
        m_messagesForLoadImages.removeFirst();
    }
    else
    {
        m_messagesForLoadImages.removeFirst();
        requestNextAttachmentImage();
    }
}

void DiscordWebProcessor::requestNextAvatarImage()
{
    if(m_messagesForLoadAvatars.isEmpty())
    {
        m_messageGroupChannelId.removeAll("AVATARS");
        requestNextMessagesGroup();
        return;
    }

    if (!checkAvatarExist(m_messagesForLoadAvatars.first()))
    {
        requestUserAvatar(m_messagesForLoadAvatars.first(), AllMesagesReceive);
        m_messagesForLoadAvatars.removeFirst();
    }
    else
    {
        m_messagesForLoadAvatars.removeFirst();
        requestNextAvatarImage();
    }
}
