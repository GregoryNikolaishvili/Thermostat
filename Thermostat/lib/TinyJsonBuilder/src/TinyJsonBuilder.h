#ifndef TINYJSONBUILDER_H
#define TINYJSONBUILDER_H

#include <Arduino.h>

class TinyJsonBuilder
{
public:
    TinyJsonBuilder(char *buffer, size_t bufferSize);

    void beginObject();
    bool addKeyValue(const __FlashStringHelper *key, const __FlashStringHelper *value);
    bool addKeyValue(const __FlashStringHelper *key, int value);
    void endObject();
    const char *getJson();

    void reset(); // Method to reset the builder

    inline bool isEmpty()
    {
        return _index == 0;
    }

private:
    char *_buffer;
    size_t _bufferSize;
    size_t _index;
    bool _firstEntry;
    bool _isComplete;

    bool addCommaIfNeeded();
};

#endif // TINYJSONBUILDER_H
