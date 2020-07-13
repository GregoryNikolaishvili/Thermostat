int setHexT(char *buffer, int T, int idx)
{
	T = T + 1000;
	idx = setHexByte(buffer, T >> 8, idx);
	idx = setHexByte(buffer, T & 0xFF, idx);
	return idx;
}

int readHexT(const char* s)
{
	int value = readHex(s, 4);
	return value - 1000;
}

int readHexInt32(const char* s)
{
	long value = 0;
	int length = 8;
	while (length > 0)
	{
		value = (value << 4) | hexCharToByte(*s++);
		length--;
	}
	return value;
}

int setHexByte(char *buffer, byte x, int idx)
{
  buffer[idx++] = byteToHexChar((x >> 4) & 0x0F);
  buffer[idx++] = byteToHexChar(x & 0x0F);
  return idx;
}

int setHexInt16(char *buffer, int x, int idx)
{
  idx = setHexByte(buffer, x >> 8, idx);
  idx = setHexByte(buffer, x & 0xFF, idx);
  return idx;
}

int setHexInt32(char *buffer, long x, int idx)
{
	idx = setHexInt16(buffer, x >> 16, idx);
	idx = setHexInt16(buffer, x & 0xFFFF, idx);
	return idx;
}

//void printHexByte(Print* client, byte x)
//{
//  client->print(byte((x >> 4) & 0x0F), HEX);
//  client->print(byte(x & 0x0F), HEX);
//}

//void printHexInt16(Print* client, int x)
//{
//  printHexByte(client, byte((x >> 8)));
//  printHexByte(client, byte(x & 0xFF));
//}

//void print2Digits(Print* client, byte x)
//{
//  if (x < 10)
//    client->print('0');
//  client->print(x);
//}

//static const char hex[] PROGMEM = "0123456789abcdef";
byte hexCharToByte(char c)
{
  if (c > '9')
    c -= ('A' - '9' - 1);
  return c - '0';
}

char byteToHexChar(byte b)
{
  if (b > 9)
    return b + 'A' - 10;
  return b + '0';
}

int readHex(const char* s, byte length)
{
  int value = 0;
  while (length > 0)
  {
    value = (value << 4) | hexCharToByte(*s++);
    length--;
  }

  return value;
}

void printDateTime(Print* client, time_t value)
{
  client->print(day(value));
  client->print(' ');
  client->print(monthShortStr(month(value)));
  client->print(' ');
  client->print(year(value));
  client->print(' ');

  if (hour(value) < 10)
    client->print('0');
  client->print(hour(value));
  client->print(':');
  if (minute(value) < 10)
    client->print('0');
  client->print(minute(value));
  client->print(':');
  if (second(value) < 10)
    client->print('0');
  client->print(second(value));
  client->println();
}

void printTime(Print* client, time_t value)
{
  if (hour(value) < 10)
    client->print('0');
  client->print(hour(value));
  client->print(':');
  if (minute(value) < 10)
    client->print('0');
  client->print(minute(value));
  client->print(':');
  if (second(value) < 10)
    client->print('0');
  client->print(second(value));
  client->print(' ');
}
