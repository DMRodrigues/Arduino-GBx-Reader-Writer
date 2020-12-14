///////////////////////////////////////////////////////////
/// Address
// A0  - PC0
// A1  - PC1
// A2  - PC2
// A3  - PC3
// A4  - PC4
// A5  - PC5
// A6  - PC6
// A7  - PC7
// A8  - PA0
// A9  - PA1
// A10 - PA2
// A11 - PA3
// A12 - PA4
// A13 - PA5
// A14 - PA6
// A15 - PA7
///////////////////////////////////////////////////////////
/// Data
// D0 - PB0
// D1 - PB1
// D2 - PB2
// D3 - PB3
// D4 - PB4
// D5 - PB5
// D6 - PB6
// D7 - PB7
///////////////////////////////////////////////////////////
/// Control
// Write      - PD4
// Read       - PD5
// ChipSelect - PD6

///////////////////////////////////////////////////////////
#define SERIAL_BAUDRATE   ( 115200 )
#define SEND_CHUNK_SIZE   ( 64     )

///////////////////////////////////////////////////////////
#define READ_HEADER_COMMAND   0x01
#define READ_ROM_COMMAND      0x02
#define READ_RAM_COMMAND      0x03
#define WRITE_RAM_COMMAND     0x04
#define GET_RAM_SIZE          0xF0

///////////////////////////////////////////////////////////
#define WritePinLow()         ( PORTD &= ~(1<<PD4)                     )
#define WritePinHigh()        ( PORTD |= (1<<PD4)                      )
#define ReadPinLow()          ( PORTD &= ~(1<<PD5)                     )
#define ReadPinHigh()         ( PORTD |= (1<<PD5)                      )
#define ChipSelectPinLow()    ( PORTD &= ~(1<<PD6)                     )
#define ChipSelectPinHigh()   ( PORTD |= (1<<PD6)                      )
#define ControlPinsLow()      ( PORTD &= ~((1<<PD4)|(1<<PD5)|(1<<PD6)) )
#define ControlPinsHigh()     ( PORTD |= ((1<<PD4)|(1<<PD5)|(1<<PD6))  )
#define ControlPinsOutput()   ( DDRD |= ((1<<PD4)|(1<<PD5)|(1<<PD6))   )
#define DataPinsInput()       ( DDRB = B00000000                       )
#define DataPinsOutput()      ( DDRB = B11111111                       )

///////////////////////////////////////////////////////////
#define GetROMBanks()         ( (RomSize >= 1 ? (2 << RomSize) : 2) )

///////////////////////////////////////////////////////////
#define GetLongFromCHAR(B)    ( ((unsigned long)B[0] << 24) | ((unsigned long)B[1] << 16) | ((unsigned long)B[2] << 8) | (unsigned long)B[3]          )
#define SetLongToCHAR(B, L)   ( B[0] = ((L & 0xFF000000LU) >> 24), B[1] = ((L & 0xFF0000LU) >> 16), B[2] = ((L & 0xFF00LU) >> 8), B[3] = (L & 0xFFLU) )

///////////////////////////////////////////////////////////
#define EnableRAM()           ( WriteByte(0x0000, 0x0A) )
#define DisableRAM()          ( WriteByte(0x0000, 0x00) )
#define SwitchRAMBank(B)      ( WriteByte(0x4000, B)    )

///////////////////////////////////////////////////////////
unsigned char CartridgeType;
unsigned char RomSize;
unsigned char RamSize;

///////////////////////////////////////////////////////////
void SendPacketSize(unsigned long L)
{
  Serial.write((L & 0xFF000000LU) >> 24);
  Serial.write((L & 0xFF0000LU  ) >> 16);
  Serial.write((L & 0xFF00LU    ) >>  8);
  Serial.write((L & 0xFFU       )      );
}

///////////////////////////////////////////////////////////
void ResetVariables()
{
  CartridgeType = 0;
  RomSize = 0;
  RamSize = 0;
}

///////////////////////////////////////////////////////////
void WriteAddress(unsigned int address)
{
  PORTC = (address & 0xFF);
  PORTA = ((address >> 8) & 0xFF);
}

///////////////////////////////////////////////////////////
unsigned char ReadByte(unsigned int address)
{
  unsigned char result;
  WriteAddress(address);
  asm volatile("nop"); // volatile to ensure optimizations don't remove it
  ChipSelectPinLow();
  ReadPinLow();
  asm volatile("nop");
  asm volatile("nop");
  asm volatile("nop");
  result = PINB;
  ReadPinHigh();
  ChipSelectPinHigh();
  return result;
}

///////////////////////////////////////////////////////////
void WriteByte(unsigned int address, unsigned char data)
{
  DataPinsOutput();
  WriteAddress(address);
  asm volatile("nop");
  PORTB = data;
  WritePinLow();
  asm volatile("nop");
  WritePinHigh();
  DataPinsInput();
}

///////////////////////////////////////////////////////////
void WriteByteRAM(unsigned int address, unsigned char data)
{
  ChipSelectPinLow();
  WriteByte(address, data);
  asm volatile("nop");
  asm volatile("nop");
  asm volatile("nop");
  ChipSelectPinHigh();
}

///////////////////////////////////////////////////////////
unsigned char ValidateChecksum()
{
  int i;
  int checksum = 0;
  for (i = 0x0134; i < 0x014E; i++) {
    checksum += ReadByte(i);
  }
  return (((checksum + 25) & 0xFF) == 0);
}

///////////////////////////////////////////////////////////
unsigned short GetRAMBanks()
{
  if (CartridgeType == 5) return 1; /* MBC2 */
  if (CartridgeType == 6) return 1; /* MBC2 */
  switch (RamSize) {
    case 0x01:
      return 1;
    case 0x02:
      return 1;
    case 0x03:
      return 4;
    case 0x04:
      return 16;
    case 0x05:
      return 8;
    default:
      return 0;
  }
}

///////////////////////////////////////////////////////////
unsigned long GetMaxAddressRAM()
{
  if (RamSize == 0) return 0;
  if (CartridgeType == 5) return 0xA200UL; /* MBC2 */
  if (CartridgeType == 6) return 0xA200UL; /* MBC2 */
  if (RamSize == 1) return 0xA800UL;
  return 0xC000UL;
}

///////////////////////////////////////////////////////////
void SwitchROMBank(unsigned short bank)
{
  if (CartridgeType >= 5) {
    WriteByte(0x2100, bank);
  }
  else {
    /* 00h: ROM only          */
    /* 01h: MBC1              */
    /* 02h: MBC1 + RAM        */
    /* 03h: MBC1 + RAM + BATT */
    WriteByte(0x6000, 0);
    WriteByte(0x4000, bank >> 5);
    WriteByte(0x2000, bank & 0x1F);
  }
}

///////////////////////////////////////////////////////////
void ReadSendHeader()
{
  unsigned int i;
  unsigned int j;
  char romInfo[32];
  char romTitle[16];

  ControlPinsHigh();

  for (i = 0x0134; i < 0x0143; i++) {
    romTitle[i - 0x0134] = ReadByte(i);
  }
  romTitle[i - 0x0134] = '\0';

  i = 0;
  romInfo[i++] = (unsigned char)strlen(romTitle);
  for (j = 0; j < strlen(romTitle); j++) {
    romInfo[i++] = romTitle[j];
  }
  romInfo[i++] = '\0';
  romInfo[i++] = CartridgeType = ReadByte(0x0147);
  romInfo[i++] = RomSize = ReadByte(0x0148);
  romInfo[i++] = RamSize = ReadByte(0x0149);
  romInfo[i++] = ReadByte(0x014C);
  romInfo[i++] = ValidateChecksum();

  ControlPinsLow();

  Serial.write(0x10);
  Serial.write(0x02);
  SendPacketSize(i);
  Serial.write(romInfo, i);
}

///////////////////////////////////////////////////////////
void ReadSendROM()
{
  unsigned short i;
  unsigned short bank;
  unsigned short romBanks = GetROMBanks();
  unsigned int romAddress;
  unsigned char sendChunk[SEND_CHUNK_SIZE];

  Serial.write(0x10);
  Serial.write(0x02);
  SendPacketSize(romBanks * 0x4000LU);

  ControlPinsHigh();

  romAddress = 0;
  SwitchROMBank(1);
  while (romAddress <= 0x7FFF) {
    for (i = 0; i < SEND_CHUNK_SIZE; i++) {
      sendChunk[i] = ReadByte(romAddress + i);
    }
    Serial.write(sendChunk, SEND_CHUNK_SIZE);
    romAddress += SEND_CHUNK_SIZE;
  }

  for (bank = 2; bank < romBanks; bank++) {
    romAddress = 0x4000;
    SwitchROMBank(bank);
    while (romAddress <= 0x7FFF) {
      for (i = 0; i < SEND_CHUNK_SIZE; i++) {
        sendChunk[i] = ReadByte(romAddress + i);
      }
      Serial.write(sendChunk, SEND_CHUNK_SIZE);
      romAddress += SEND_CHUNK_SIZE;
    }
  }

  ControlPinsLow();
}

///////////////////////////////////////////////////////////
void ReadSendRAM()
{
  unsigned char bank;
  unsigned char ramBanks;
  unsigned short i;
  unsigned long ramAddress;
  unsigned long ramMaxAddress;
  unsigned char sendChunk[SEND_CHUNK_SIZE];

  Serial.write(0x10);
  Serial.write(0x02);
  if (RamSize == 0) {
    SendPacketSize(0);
    return;
  }

  ramBanks = GetRAMBanks();
  ramMaxAddress = GetMaxAddressRAM();
  SendPacketSize(ramBanks * (ramMaxAddress - 0xA000UL));

  ControlPinsHigh();

  // some MBC2 fix apparently needed
  ReadByte(0x0134);

  // some MBC1 fix apparently needed, to set RAM mode
  if (CartridgeType <= 4) WriteByte(0x6000, 1);

  EnableRAM();

  for (bank = 0; bank < ramBanks; bank++) {
    ramAddress = 0xA000;
    SwitchRAMBank(bank);
    while (ramAddress < ramMaxAddress) {
      for (i = 0; i < SEND_CHUNK_SIZE; i++) {
        sendChunk[i] = ReadByte(ramAddress + i);
      }
      Serial.write(sendChunk, SEND_CHUNK_SIZE);
      ramAddress += SEND_CHUNK_SIZE;
    }
  }

  DisableRAM();

  ControlPinsLow();
}

///////////////////////////////////////////////////////////
void RecvWriteRAM()
{
  unsigned char bank;
  unsigned char ramBanks;
  unsigned long ramAddress;
  unsigned long ramMaxAddress;

  if (RamSize == 0) return;

  ramBanks = GetRAMBanks();
  ramMaxAddress = GetMaxAddressRAM();

  ControlPinsHigh();

  // some MBC2 fix apparently needed
  ReadByte(0x0134);

  // some MBC1 fix apparently needed, to set RAM mode
  if (CartridgeType <= 4) WriteByte(0x6000, 1);

  EnableRAM();

  for (bank = 0; bank < ramBanks; bank++) {
    ramAddress = 0xA000;
    SwitchRAMBank(bank);
    while (ramAddress < ramMaxAddress) {
      unsigned char data;
      while (Serial.available() <= 0);
      data = Serial.read();
      WriteByteRAM(ramAddress, data);
      ramAddress++;
    }
  }

  DisableRAM();

  ControlPinsLow();
}

///////////////////////////////////////////////////////////
void setup()
{
  DDRC = B11111111;
  DDRA = B11111111;
  ControlPinsOutput();
  DataPinsInput();

  PORTA = B00000000;
  PORTC = B00000000;
  ControlPinsLow();

  Serial.begin(SERIAL_BAUDRATE);

  ResetVariables();
}

///////////////////////////////////////////////////////////
void loop()
{
  unsigned char HasDLE = 0;
  unsigned char HasSTX = 0;
  unsigned char inputOK = 0;
  unsigned char command = 0;

  while (Serial.available() <= 6) {
    _delay_ms(250);
  }
  
  /* Need: DLE + STX + SIZE(4) + CMD */
  do {
    HasDLE = (Serial.read() == 0x10);
  } while (!HasDLE && Serial.available());
  if (/*HasDLE && */Serial.available()) HasSTX = (Serial.read() == 0x02);
  if (HasDLE && HasSTX && (Serial.available() == 5)) {
    unsigned long theSize;
    unsigned char packetSize[4];
    packetSize[0] = Serial.read();
    packetSize[1] = Serial.read();
    packetSize[2] = Serial.read();
    packetSize[3] = Serial.read();
    theSize = GetLongFromCHAR(packetSize);
    if (theSize == 1) {
      inputOK = 1;
      command = Serial.read();
    }
  }
  else {
    while (Serial.available()) Serial.read(); /* discard */
  }

  /* Process */
  if (inputOK) {
    switch (command) {
      case READ_HEADER_COMMAND:
        ResetVariables();
        ReadSendHeader();
        break;
      case READ_ROM_COMMAND:
        /* We need: CartridgeType + RomSize */
        ReadSendROM();
        break;
      case READ_RAM_COMMAND:
        /* We need: CartridgeType + RamSize */
        ReadSendRAM();
        break;
      case WRITE_RAM_COMMAND:
        RecvWriteRAM();
        break;
      case GET_RAM_SIZE:
        Serial.write(0x10);
        Serial.write(0x02);
        SendPacketSize(4);
        if (RamSize == 0) {
          SendPacketSize(0);
        }
        else {
          unsigned char ramBanks = GetRAMBanks();
          unsigned long ramMaxAddress = GetMaxAddressRAM();
          SendPacketSize(ramBanks * (ramMaxAddress - 0xA000UL));
        }
        break;
      default:
        break;
    }
  }
  else {
    unsigned char BAD_CMD[6] = { 0x10, 0x02, 0x00, 0x00, 0x00, 0x00 };
    Serial.write(BAD_CMD, 6);
  }

  Serial.flush();
}
