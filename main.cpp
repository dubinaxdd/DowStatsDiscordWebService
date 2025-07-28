#include <QCoreApplication>
#include "DiscordWebProcessor.h"
#include "NewsServer.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    DiscorsWebProcessor discorsWebProcessor;
    NewsServer newsServer;

    QObject::connect(&discorsWebProcessor, &DiscorsWebProcessor::sendEvent, &newsServer, &NewsServer::onEventReceived , Qt::QueuedConnection );

    newsServer.setNewsMessagesListPtr(discorsWebProcessor.newsMessages());
    newsServer.setEventsMessagesListPtr(discorsWebProcessor.eventMessages());
    newsServer.setTestMessagesListPtr(discorsWebProcessor.testMessages());

    return a.exec();
}
