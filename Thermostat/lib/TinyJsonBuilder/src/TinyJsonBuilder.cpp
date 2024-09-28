#include "TinyJsonBuilder.h"

TinyJsonBuilder::TinyJsonBuilder(char *buffer, size_t bufferSize)
    : _buffer(buffer), _bufferSize(bufferSize), _index(0), _firstEntry(true), _isComplete(false)
{
    // Initialize buffer to empty string
    if (_bufferSize > 0)
    {
        _buffer[0] = '\0';
    }
}

void TinyJsonBuilder::reset()
{
    _index = 0;
    _firstEntry = true;
    _isComplete = false;
    if (_bufferSize > 0)
    {
        _buffer[0] = '\0';
    }
}

void TinyJsonBuilder::beginObject()
{
    _index = 0;
    _firstEntry = true;
    _isComplete = false;

    // Add opening brace
    if (_bufferSize > 1)
    {
        _buffer[_index++] = '{';
        _buffer[_index] = '\0';
    }
}

bool TinyJsonBuilder::addKeyValue(const __FlashStringHelper *key, const __FlashStringHelper *value)
{
    if (_isComplete)
        return false;

    if (!addCommaIfNeeded())
        return false;

    // Copy key and value from PROGMEM to RAM buffers
    char keyBuffer[16];   // Adjust size as needed
    char valueBuffer[64]; // Adjust size as needed

    strncpy_P(keyBuffer, (PGM_P)key, sizeof(keyBuffer));
    keyBuffer[sizeof(keyBuffer) - 1] = '\0';

    strncpy_P(valueBuffer, (PGM_P)value, sizeof(valueBuffer));
    valueBuffer[sizeof(valueBuffer) - 1] = '\0';

    // Add key-value pair without escaping
    int written = snprintf(&_buffer[_index], _bufferSize - _index, "\"%s\":\"%s\"", keyBuffer, valueBuffer);
    if (written < 0 || (size_t)written >= _bufferSize - _index)
        return false;

    _index += written;
    _buffer[_index] = '\0';

    return true;
}

bool TinyJsonBuilder::addKeyValue(const __FlashStringHelper *key, int value)
{
    if (_isComplete)
        return false;

    if (!addCommaIfNeeded())
        return false;

    // Copy key from PROGMEM to RAM buffer
    char keyBuffer[16]; // Adjust size as needed

    strncpy_P(keyBuffer, (PGM_P)key, sizeof(keyBuffer));
    keyBuffer[sizeof(keyBuffer) - 1] = '\0';

    // Add key-value pair for integer
    int written = snprintf(&_buffer[_index], _bufferSize - _index, "\"%s\":%d", keyBuffer, value);
    if (written < 0 || (size_t)written >= _bufferSize - _index)
        return false;

    _index += written;
    _buffer[_index] = '\0';

    return true;
}

void TinyJsonBuilder::endObject()
{
    if (_isComplete)
        return;

    if (_index + 1 >= _bufferSize)
        return;
    _buffer[_index++] = '}';
    _buffer[_index] = '\0';
    _isComplete = true;
}

const char *TinyJsonBuilder::getJson()
{
    return _buffer;
}

bool TinyJsonBuilder::addCommaIfNeeded()
{
    if (_index + 1 >= _bufferSize)
        return false;

    if (!_firstEntry)
    {
        _buffer[_index++] = ',';
    }
    else
    {
        _firstEntry = false;
    }
    return true;
}
