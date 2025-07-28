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
    AllMesagesReceive = 8
};

#endif // BASETYPES_H
