#ifndef BASETYPES_H
#define BASETYPES_H

enum EventType: int
{
    LastMessagesIdAnswer = 1,
    CreateNewsMessage = 2,
    UpdateNewsMessage = 3,
    DeleteNewsMessage = 4,
    CreateEventMessage = 5,
    UpdateEventMessage = 6,
    DeleteEventMessage = 7,
    CreateTestMessage = 8,
    UpdateTestMessage = 9,
    DeleteTestMessage = 10,
    AllMesagesReceive = 11,
    NewsMessagesAnswer = 12,
    EventsMessagesAnswer = 13,
    TestMessagesAnswer = 14,
    NewsMessagesEnd = 15,
    EventsMessagesEnd = 16,
    TestMessagesEnd = 17,
    PingResponce = 18
};

#endif // BASETYPES_H
