#ifndef BASETYPES_H
#define BASETYPES_H

enum EventType: int
{
    LastMessagesIdAnswer = 1,
    CreateNewsMessage = 2,
    UpdateNewsMessage = 3,
    CreateEventMessage = 4,
    UpdateEventMessage = 5,
    CreateTestMessage = 6,
    UpdateTestMessage = 7,
    AllMesagesReceive = 8,
    NewsMessagesAnswer = 9,
    EventsMessagesAnswer = 10,
    TestMessagesAnswer = 11,
    NewsMessagesEnd = 12,
    EventsMessagesEnd = 13,
    TestMessagesEnd = 14
};

#endif // BASETYPES_H
