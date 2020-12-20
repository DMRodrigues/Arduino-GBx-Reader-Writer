/*
 * DMRodrigues, 2020
 * Code for connecting to an Arduino running 'arduino-cartridge-rw' to read or write Game Boy cartridges.
 *
 * Execute with --help to see instructions.
 *
 */
#if defined(_WIN32) || defined(_WIN64)
#define _CRT_SECURE_NO_WARNINGS
#endif /* _WIN32 || _WIN64 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>

#else
#include <unistd.h>
#include <termios.h>
#include <getopt.h>
#include <sys/ioctl.h>

#if __APPLE__
#include <IOKit/serial/ioss.h>
#endif /* __APPLE__ */

#endif /* _WIN32 || _WIN64 */


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
#define SERIAL_BAUDRATE   ( 500000 )
#define SERIAL_TIMEOUT    ( 3      ) /* seconds */
#define SEND_CHUNK_SIZE   ( 32     )
#define RECV_CHUNK_SIZE   ( 512    )

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
#define READ_HEADER_COMMAND   0x01
#define READ_ROM_COMMAND      0x02
#define READ_RAM_COMMAND      0x03
#define WRITE_RAM_COMMAND     0x04
#define GET_RAM_SIZE          0xF0

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
static int ctrlc = 0;
static unsigned char verbose = 0;

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
static char rom_title[16];

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
#define print_state_console(S,D)   ( printf("\rState: %ld of %ld (%.1f%%)", D, S, (((double)D / (double)S) * 100)), fflush(stdout) )

///////////////////////////////////////////////////////////
#define long_from_array(B)    ( (((unsigned long)B[0] << 24) | ((unsigned long)B[1] << 16) | ((unsigned long)B[2] << 8) | (unsigned long)B[3])        )
#define long_to_array(B, L)   ( B[0] = ((L & 0xFF000000LU) >> 24), B[1] = ((L & 0xFF0000LU) >> 16), B[2] = ((L & 0xFF00LU) >> 8), B[3] = (L & 0xFFLU) )

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
#if defined(_WIN32) || defined(_WIN64)
#define wait_ms   Sleep

#define get_time()      ( GetTickCount()                               )
#define time_valid(S)   ( ((get_time() - S) < (SERIAL_TIMEOUT * 1000)) )

#else
static void wait_ms(unsigned int in_milliseconds)
{
  usleep(in_milliseconds * 1000UL);
}

static unsigned long get_time()
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (unsigned long)((ts.tv_nsec / 1000000) + (ts.tv_sec * 1000UL));
}

#define time_valid(S)   ( ((get_time() - S) < (SERIAL_TIMEOUT * 1000)) )

#endif /* _WIN32 || _WIN64 */

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
#if defined(_WIN32) || defined(_WIN64)
BOOL WINAPI handle_sig(DWORD signum)
{
  switch (signum) {
    case CTRL_C_EVENT:
      ctrlc = 1;
      return TRUE;
    default:
      break;
  }
  return FALSE;
}

#else
void handle_sig(int signum)
{
  ctrlc = 1;
}

struct sigaction int_handler = {
  .sa_handler = handle_sig,
};

#endif /* _WIN32 || _WIN64 */

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
#if defined(_WIN32) || defined(_WIN64)
typedef SSIZE_T ssize_t;

static ssize_t read(HANDLE fd, void *buf, size_t count)
{
  DWORD NumberOfBytesRead;
  ReadFile(fd, buf, count, &NumberOfBytesRead, NULL);
  return NumberOfBytesRead;
}

static ssize_t write(HANDLE fd, const void *buf, size_t count)
{
  DWORD NumberOfBytesRead;
  WriteFile(fd, buf, count, &NumberOfBytesRead, NULL);
  return NumberOfBytesRead;
}

#define flush_serial(fd)   ( PurgeComm(fd, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR) )

#else
#define HANDLE   int

#define flush_serial(fd)   ( tcflush(fd, TCIOFLUSH) )

#endif /* _WIN32 || _WIN64 */

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
static unsigned char get_file_size(const char *path, ssize_t *out_size)
{
  struct stat tmp;
  *out_size = 0;
  if (stat(path, &tmp) != 0) return 1;
  *out_size = (ssize_t)tmp.st_size;
  return 0;
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
static void print_packet(const unsigned char *packet, long packet_size)
{
  long i;
  for (i = 0; i < packet_size; i++) {
    printf("%02X ", packet[i]);
  }
  printf("\n");
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
static void print_usage(const char *program_name)
{
#if defined(_WIN32) || defined(_WIN64)
  printf("\nUsage: %s <port> [OPTIONS...]\n", program_name);
  printf("\n");
  printf("  -v            print debug.\n");
  printf("\nExample:\n");
  printf("  %s COM9\n", program_name);
#else
  printf("\nUsage: %s -p <port> [OPTIONS...]\n", program_name);
  printf("\n");
  printf("  -v, --verbose            print debug.\n");
  printf("  -h, --help               print this screen.\n");
  printf("\nExample:\n");
  printf("  %s -p /dev/ttyACM0\n", program_name);
#endif /* _WIN32 || _WIN64 */
  printf("\n");
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
static const char* print_cartridge_string(unsigned char cartridge_type)
{
  switch (cartridge_type) {
    case 0x00:   return("00h - ROM ONLY");
    case 0x01:   return("01h - MBC1");
    case 0x02:   return("02h - MBC1+RAM");
    case 0x03:   return("03h - MBC1+RAM+BATTERY");
    case 0x05:   return("05h - MBC2");
    case 0x06:   return("06h - MBC2+BATTERY");
    case 0x08:   return("08h - ROM+RAM");
    case 0x09:   return("09h - ROM+RAM+BATTERY");
    case 0x0B:   return("08h - MMM01");
    case 0x0C:   return("0Ch - MMM01+RAM");
    case 0x0D:   return("0Dh - MMM01+RAM+BATTERY");
    case 0x0F:   return("0Fh - MBC3+TIMER+BATTERY");
    case 0x10:   return("10h - MBC3+TIMER+RAM+BATTERY");
    case 0x11:   return("11h - MBC3");
    case 0x12:   return("12h - MBC3+RAM");
    case 0x13:   return("13h - MBC3+RAM+BATTERY");
    case 0x19:   return("19h - MBC5");
    case 0x1A:   return("1Ah - MBC5+RAM");
    case 0x1B:   return("1Bh - MBC5+RAM+BATTERY");
    case 0x1C:   return("1Ch - MBC5+RUMBLE");
    case 0x1D:   return("1Dh - MBC5+RUMBLE+RAM");
    case 0x1E:   return("1Eh - MBC5+RUMBLE+RAM+BATTERY");
    case 0x20:   return("20h - MBC6");
    case 0x22:   return("22h - MBC7+SENSOR+RUMBLE+RAM+BATTERY");
    case 0xFC:   return("FCh - POCKET CAMERA");
    case 0xFD:   return("FDh - BANDAI TAMA5");
    case 0xFE:   return("FEh - HuC3");
    case 0xFF:   return("FFh - HuC1+RAM+BATTERY");
    default:     break;
  }
  return("UNKNOW");
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
static const char* print_rom_size(unsigned char rom_size)
{
  switch (rom_size) {
    case 0x00:   return("00h - 32KByte(no ROM banking)");
    case 0x01:   return("01h - 64KByte(4 banks)");
    case 0x02:   return("02h - 128KByte(8 banks)");
    case 0x03:   return("03h - 256KByte(16 banks)");
    case 0x04:   return("04h - 512KByte(32 banks)");
    case 0x05:   return("05h - 1MByte(64 banks) => only 63 banks used by MBC1");
    case 0x06:   return("06h - 2MByte(128 banks) => only 125 banks used by MBC1");
    case 0x07:   return("07h - 4MByte(256 banks)");
    case 0x08:   return("08h - 8MByte(512 banks)");
    case 0x52:   return("52h - 1.1MByte(72 banks)");
    case 0x53:   return("53h - 1.2MByte(80 banks)");
    case 0x54:   return("54h - 1.5MByte(96 banks)");
    default:     break;
  }
  return("UNKNOW");
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
static const char* print_ram_size(unsigned char ram_size)
{
  switch (ram_size) {
    case 0x00:   return("00h - None");
    case 0x01:   return("01h - 2 KBytes");
    case 0x02:   return("02h - 8 Kbytes");
    case 0x03:   return("03h - 32 KBytes(4 banks of 8KBytes each)");
    case 0x04:   return("04h - 128 KBytes(16 banks of 8KBytes each)");
    case 0x05:   return("05h - 64 KBytes(8 banks of 8KBytes each)");
    default:     break;
  }
  return("UNKNOW");
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
static unsigned char send_packet_routine(HANDLE fd, unsigned char cmd)
{
  int i;
  unsigned char tx_packet[7];

  if (verbose) printf("send_packet_routine\n");

  i = 0;
  tx_packet[i++] = 0x10;
  tx_packet[i++] = 0x02;
  tx_packet[i++] = 0x00;
  tx_packet[i++] = 0x00;
  tx_packet[i++] = 0x00;
  tx_packet[i++] = 0x01;
  tx_packet[i++] = cmd;

  if (verbose) print_packet(tx_packet, sizeof(tx_packet));
  if (write(fd, tx_packet, sizeof(tx_packet)) != sizeof(tx_packet)) {
    printf("Error sending command: %s\n", strerror(errno));
    return 1;
  }

  return 0;
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
static ssize_t recv_packet_header_size(HANDLE fd)
{
  ssize_t ret;
  ssize_t available;
  unsigned char c;
  unsigned char has_dle;
  unsigned char has_stx;
  unsigned long start;

  if (verbose) printf("recv_packet_header_size\n");

  /* receive the header of packet */
  c = 0;
  has_dle = 0;
  has_stx = 0;
  start = get_time();
  do {
    ret = read(fd, &c, 1);
    if (verbose && (ret > 0)) printf("RECEIVED 1: %X\n", c);
  } while ((c != 0x10) && !ctrlc && time_valid(start));
  c = 0;
  ret = 0;
  has_dle = 1;
  do {
    ret = read(fd, &c, 1);
    if (ret > 0) {
      if (verbose) printf("RECEIVED 2: %X\n", c);
      if (c == 0x02) {
        has_stx = 1;
      }
      break;
    }
  } while (!ctrlc && time_valid(start));

  if (ctrlc) return -1;
  if (!has_dle || !has_stx) {
    printf("TIMEOUT: DLE and/or STX not received!\n");
    return -2;
  }

  /* receive the size */
  available = -1;
  {
    ssize_t i = 0;
    ssize_t j = 4;
    unsigned char buff_size[4];
    start = get_time();
    do {
      ret = read(fd, buff_size + i, j);
      i += ret;
      j -= ret;
    } while ((j > 0) && !ctrlc && time_valid(start));
    if (i == 4) {
      available = long_from_array(buff_size);
      if (verbose) printf("Received packet: %d %d %d %d => Total: %ld\n", buff_size[0], buff_size[1], buff_size[2], buff_size[3], available);
    }
  }

  if (ctrlc) return -1;
  if (available == -1) {
    printf("Error receiving packet size\n");
    return -3;
  }

  return available;
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
static unsigned char recv_routine_buffer(HANDLE fd, ssize_t packet_size, unsigned char *out_buff, ssize_t out_buff_size, unsigned char print_state)
{
  ssize_t ret;
  ssize_t start;
  ssize_t offset;
  const ssize_t _packet_size = packet_size;

  if (verbose) printf("recv_routine_buffer\n");

  offset = 0;
  start = get_time();
  do {
    ret = read(fd, out_buff + offset, packet_size);
    if (ret > 0) {
      if ((offset + ret) > out_buff_size) {
        printf("Not enough space in buffer!\n");
        return 4;
      }
      offset += ret;
      packet_size -= ret;
      start = get_time();
    }
    else if (ret == 0) {
      wait_ms(10); /* let's wait */
    }
    else {
      printf("Nasty: %s\n", strerror(errno));
      return 3;
    }
    if (print_state) print_state_console(_packet_size, (_packet_size - packet_size));

  } while ((packet_size > 0) && !ctrlc && time_valid(start));
  if (print_state) printf("\n");

  if (ctrlc) return 1;
  if (packet_size > 0) {
    printf("ERROR: missing data!!!\n");
    return 2;
  }

  return 0;
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
static unsigned char recv_routine_file(HANDLE fd, ssize_t packet_size, FILE *fp, unsigned char print_state)
{
  ssize_t ret;
  ssize_t start;
  unsigned char rx_chunk[RECV_CHUNK_SIZE];
  const ssize_t _packet_size = packet_size;

  if (verbose) printf("recv_routine_file\n");

  start = get_time();
  do {
    ret = read(fd, rx_chunk, RECV_CHUNK_SIZE);
    if (ret > 0) {
      if (fwrite(rx_chunk, 1, ret, fp) != ret) {
        printf("Error writing to file: %s\n", strerror(errno));
        return 4;
      }
      packet_size -= ret;
      start = get_time();
    }
    else if (ret == 0) {
      wait_ms(5); /* let's wait */
    }
    else {
      printf("Nasty: %s\n", strerror(errno));
      return 3;
    }
    if (print_state) print_state_console(_packet_size, (_packet_size - packet_size));
    
  } while ((packet_size > 0) && !ctrlc && time_valid(start));
  if (print_state) printf("\n");
  
  if (ctrlc) return 1;
  if (packet_size > 0) {
    printf("ERROR: missing data!!!\n");
    return 2;
  }

  return 0;
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
static unsigned char send_routine_file(HANDLE fd, FILE *fp, ssize_t file_size, unsigned char print_state)
{
  ssize_t current_size = 0;
  unsigned char tx_chunk[SEND_CHUNK_SIZE];

  if (verbose) printf("send_routine_file\n");

  do {
    if (fread(tx_chunk, 1, SEND_CHUNK_SIZE, fp) != SEND_CHUNK_SIZE) {
      printf("Error reading from file: %s\n", strerror(errno));
      break;
    }
    if (write(fd, tx_chunk, SEND_CHUNK_SIZE) != SEND_CHUNK_SIZE) {
      printf("Error sending packet: %s\n", strerror(errno));
      break;
    }
    current_size += SEND_CHUNK_SIZE;
    wait_ms(10); /* let's wait */
    if (print_state) print_state_console(file_size, current_size);
  } while (!ctrlc && (current_size < file_size));
  if (print_state) printf("\n");

  if (ctrlc) return 1;
  if (current_size < file_size) {
    printf("ERROR: missing data!!!\n");
    return 2;
  }

  return 0;
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
static void verify_ram(HANDLE fd, FILE *fp, unsigned long ram_size)
{
  char option;
  char clear_option;
  unsigned char *RAM_read;
  unsigned char *RAM_file;

  if (verbose) printf("verify_ram\n");

  printf("Verify RAM?[y/n]? ");
  option = getchar();
  do { clear_option = getchar(); } while (clear_option != '\n');

  if (option != 'y') return;
  printf("Reading RAM\n");

  flush_serial(fd);

  if (send_packet_routine(fd, READ_RAM_COMMAND)) return;

  /* UGLY => TODO: compare chunks of data */
  RAM_read = (unsigned char *)malloc(ram_size); // assume success
  RAM_file = (unsigned char *)malloc(ram_size); // assume success

  /* go back to begin of file */
  rewind(fp);
  fread(RAM_file, 1, ram_size, fp); // assume success

  if (recv_packet_header_size(fd) == ram_size) {
    if (recv_routine_buffer(fd, ram_size, RAM_read, ram_size, verbose) == 0) {
      if (memcmp(RAM_read, RAM_file, ram_size) != 0) {
        printf("=> RAM NOK(possibly corrupted)!\n");
      }
      else {
        printf("=> RAM OK!\n");
      }
    }
    else {
      printf("=> Error with RAM, try again\n");
    }
  }
  else {
    printf("=> Error with RAM, try again\n");
  }

  free(RAM_file);
  free(RAM_read);
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
static unsigned char get_ram_size(HANDLE fd, ssize_t *out_ram_size)
{
  ssize_t size;
  unsigned char ram_info[4];
  
  if (verbose) printf("get_ram_size\n");

  flush_serial(fd);

  if (send_packet_routine(fd, GET_RAM_SIZE)) return 1;

  wait_ms(10); /* give some time */

  size = recv_packet_header_size(fd);
  if (size > 0) {
    if (recv_routine_buffer(fd, size, ram_info, sizeof(ram_info), 0)) return 2;
  }
  else {
    /* We must have size */
    printf("Error got no packet size!\n");
    return 3;
  }

  *out_ram_size = long_from_array(ram_info);
  if (verbose) printf("Got RAM size: %ld\n", *out_ram_size);

  return 0;
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
static void read_header(HANDLE fd, unsigned char to_print)
{
  ssize_t size;
  unsigned char ChecksumOK;
  char info[32];

  if (verbose) printf("read_header\n");

  flush_serial(fd);

  if (send_packet_routine(fd, READ_HEADER_COMMAND)) goto L_END_READ_HEADER;

  wait_ms(10); /* give some time */

  size = recv_packet_header_size(fd);
  if (size <= 0) {
    /* We must have size */
    printf("Error got no packet size!\n");
    goto L_END_READ_HEADER;
  }

  if (recv_routine_buffer(fd, size, (unsigned char *)info, sizeof(info), to_print)) goto L_END_READ_HEADER;

  ChecksumOK = info[info[0] + 1 + 5];
  if (ChecksumOK) {
    if (to_print) {
      printf("Rom title: %s\n", info + 1);
      printf("Cartridge type: %s\n", print_cartridge_string(info[info[0] + 1 + 1]));
      printf("Rom size: %s\n", print_rom_size(info[info[0] + 1 + 2]));
      printf("Ram size: %s\n", print_ram_size(info[info[0] + 1 + 3]));
      printf("Rom version: %d\n", info[info[0] + 1 + 4]);
      printf("Checksum: %d\n", info[info[0] + 1 + 5]);
    }
    memcpy(rom_title, info + 1, info[0] + 1);
  }
  else {
    printf("No cartridge inserted or cartridge read failed!\n");
  }

L_END_READ_HEADER:
  if (to_print) printf("\n");
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
static void read_rom(HANDLE fd)
{
  int i;
  FILE *fp;
  ssize_t size;
  char rom_filename[32];

  if (verbose) printf("read_rom\n");

  if (rom_title[0] == 0) {
    printf("Error no cartridge info\n");
    goto L_END_READ_ROM;
  }

  i = strlen(rom_title);
  memcpy(rom_filename, rom_title, i + 1);
  rom_filename[i++] = '.';
  rom_filename[i++] = 'g';
  rom_filename[i++] = 'b';
  rom_filename[i++] = '\0';

  printf("Reading ROM and saving to %s\n", rom_filename);

  fp = fopen(rom_filename, "wb");
  if (!fp) {
    printf("Error creating %s: %s\n", rom_filename, strerror(errno));
    goto L_END_READ_ROM;
  }

  flush_serial(fd);

  if (send_packet_routine(fd, READ_ROM_COMMAND)) {
    fclose(fp);
    goto L_END_READ_ROM;
  }

  wait_ms(10); /* give some time */

  size = recv_packet_header_size(fd);
  if (size > 0) {
    recv_routine_file(fd, size, fp, 1);
  }
  else {
    /* We must have size */
    printf("Error got no packet size!\n");
  }

  fclose(fp);

L_END_READ_ROM:
  printf("\n");
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
static void read_ram(HANDLE fd)
{
  int i;
  FILE *fp;
  ssize_t size;
  char ram_filename[32];

  if (verbose) printf("read_ram\n");

  if (rom_title[0] == 0) {
    printf("Error no cartridge info\n");
    goto L_END_READ_RAM;
  }

  i = strlen(rom_title);
  memcpy(ram_filename, rom_title, i + 1);
  ram_filename[i++] = '.';
  ram_filename[i++] = 's';
  ram_filename[i++] = 'a';
  ram_filename[i++] = 'v';
  ram_filename[i++] = '\0';

  printf("Reading RAM and saving to %s\n", ram_filename);

  fp = fopen(ram_filename, "wb");
  if (!fp) {
    printf("Error creating %s: %s\n", ram_filename, strerror(errno));
    goto L_END_READ_RAM;
  }

  flush_serial(fd);

  if (send_packet_routine(fd, READ_RAM_COMMAND)) {
    fclose(fp);
    goto L_END_READ_RAM;
  }

  wait_ms(10); /* give some time */

  size = recv_packet_header_size(fd);
  if (size > 0) {
    recv_routine_file(fd, size, fp, 1);
  }
  else {
    /* We must have size */
    printf("Error got no packet size!\n");
  }

  fclose(fp);

L_END_READ_RAM:
  printf("\n");
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
static void write_ram(HANDLE fd)
{
  FILE *fp;
  ssize_t ram_size;
  char ram_filename[32];

  if (verbose) printf("write_ram\n");

  if (rom_title[0] == 0) {
    printf("Error no cartridge info\n");
    goto L_END_WRITE_RAM;
  }

  if (get_ram_size(fd, &ram_size)) {
    printf("Error try again\n");
    goto L_END_WRITE_RAM;
  }

  if (ram_size > 0) {
    int i;
    char option;
    char clear_option;
    ssize_t compare_size;

    i = strlen(rom_title);
    memcpy(ram_filename, rom_title, i + 1);
    ram_filename[i++] = '.';
    ram_filename[i++] = 's';
    ram_filename[i++] = 'a';
    ram_filename[i++] = 'v';
    ram_filename[i++] = '\0';

    if (get_file_size(ram_filename, &compare_size)) {
      printf("No file found or couldn't open file\n");
      goto L_END_WRITE_RAM;
    }
    if (ram_size != compare_size) {
      printf("RAM file cannot be used!\n");
      goto L_END_WRITE_RAM;
    }

    printf("Use RAM file %s[y/n]? ", ram_filename);
    option = getchar();
    do { clear_option = getchar(); } while (clear_option != '\n');

    if (option != 'y') {
      printf("No action done!\n");
      goto L_END_WRITE_RAM;
    }
  }

  fp = fopen(ram_filename, "rb");
  if (!fp) {
    printf("Error openning %s: %s\n", ram_filename, strerror(errno));
    goto L_END_WRITE_RAM;
  }

  flush_serial(fd);

  if (send_packet_routine(fd, WRITE_RAM_COMMAND)) {
    fclose(fp);
    goto L_END_WRITE_RAM;
  }

  wait_ms(50); /* give some time */

  if (send_routine_file(fd, fp, ram_size, 1)) {
    fclose(fp);
    goto L_END_WRITE_RAM;
  }

  verify_ram(fd, fp, ram_size);

  fclose(fp);

L_END_WRITE_RAM:
  printf("\n");
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
int main(int argc, char *argv[])
{
  HANDLE fd;
  int next_option;

#if defined(_WIN32) || defined(_WIN64)
  if (argc < 2) {
    printf("\nNO ARGUMENTS PROVIDED!\n");
    print_usage(argv[0]);
    return EXIT_FAILURE;
  }

  fd = CreateFileA(argv[1], GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
  if (fd == INVALID_HANDLE_VALUE) {
    printf("Error opening %s: %s\n", argv[1], strerror(errno));
    return EXIT_FAILURE;
  }
  else {
    DCB dcb = { 0 };
    COMMTIMEOUTS tmo = { MAXDWORD, 0, 0, 0, 0 };
    
    memset(&dcb, 0, sizeof(DCB));
    dcb.DCBlength = sizeof(DCB);
    if (GetCommState(fd, &dcb) == FALSE) {
      CloseHandle(fd);
      exit(1);
    }
    
    dcb.BaudRate = SERIAL_BAUDRATE;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity = NOPARITY;
    dcb.fAbortOnError = FALSE;
    if (SetCommState(fd, &dcb) == FALSE) {
      CloseHandle(fd);
      exit(1);
    }
    
    if (SetCommTimeouts(fd, &tmo) == FALSE) {
      CloseHandle(fd);
      exit(1);
    }
  }
  if ((argc > 2) && strstr(argv[2], "-v")) verbose = 1;

  SetConsoleCtrlHandler(handle_sig, TRUE);

#else
  extern char *optarg;
  const char* short_options = "p:vh";
  const struct option long_options[] = {
    { "port",         required_argument, NULL, 'p' },
    { "verbose",      no_argument,       NULL, 'v' },
    { "help",         no_argument,       NULL, 'h' },
    { 0,              0,                 0,     0  }
  };

  char *port_name = NULL;
  struct termios port_attr;
  struct termios port_attr_orig;
  speed_t speed = SERIAL_BAUDRATE;

  /* no argument provided */
  if (argc == 1) {
    printf("\nNO ARGUMENTS PROVIDED!\n");
    print_usage(argv[0]);
    return EXIT_FAILURE;
  }

  do {
    next_option = getopt_long(argc, argv, short_options, long_options, NULL);
    switch (next_option) {
      case 'p':
        port_name = optarg;
        break;
      case 'v':
        verbose = 1;
        break;
      case 'h':
        print_usage(argv[0]);
        return EXIT_SUCCESS;
      case '?': /* If getopt() encounters an option character that was not in optstring, then '?' is returned */
        print_usage(argv[0]);
        return EXIT_FAILURE;
      case -1: /* If all command-line options have been parsed, then getopt() returns -1 */
        break;
      default:
        printf("\nERROR PROCESSING ARGUMENTS!\n");
        return EXIT_FAILURE;
    }
  } while (next_option != -1);

  /* check if setup parameters given and valid */
  if (!port_name) {
    printf("\nSorry, no device provided.\n\n");
    print_usage(argv[0]);
    return EXIT_FAILURE;
  }

  /* Install CTRL^C signal handler */
  sigaction(SIGINT, &int_handler, 0);
  
  /* Open serial port */
  fd = open(port_name, O_RDWR);
  if (fd == -1) {
    printf("Error opening %s: %s\n", port_name, strerror(errno));
    return EXIT_FAILURE;
  }
  if (verbose) printf("%s opened.\n", port_name);

  /* Get port config and make backup */
  tcgetattr(fd, &port_attr);
  memcpy(&port_attr_orig, &port_attr, sizeof(struct termios));

  port_attr.c_cc[VMIN] = 0; /* Return always (non-block) */
  port_attr.c_cc[VTIME] = 0; /* Never return from read due to timeout */

  port_attr.c_iflag &= ~(ICRNL | IXON);
  port_attr.c_oflag &= ~OPOST;
  port_attr.c_lflag &= ~(ISIG | ICANON | IEXTEN | ECHO | ECHOE | ECHOK | ECHOCTL | ECHOKE);

  if (tcsetattr(fd, TCSANOW, &port_attr) == -1) {
    printf("Error while setting %s options: %s\n", port_name, strerror(errno));
    close(fd);
    return EXIT_FAILURE;
  }

#if __APPLE__
  ioctl(fd, IOSSIOSPEED, &speed);

#else
  cfsetispeed(&port_attr, speed);
  cfsetospeed(&port_attr, speed);

#endif /* __APPLE__ */

  if (verbose) printf("%s successfully configured.\n", port_name);

#endif /* _WIN32 || _WIN64 */

  printf("Setting everything up\n");
  wait_ms(1200); /* delay after arduino reset */

  do {
    char clear_option;

    printf("#=========================================================#\n");
    printf("#=============== Arduino-GBx-Reader-Writer ===============#\n");
    printf("0) Read Cartidge Header\n");
    printf("1) Read ROM\n");
    printf("2) Read RAM\n");
    printf("3) Write RAM\n");
    printf("4) EXIT\n");
    printf("Select an option: ");
    next_option = getchar();
    if ((next_option < 48) || (next_option > 57)) {
      printf("Invalid option\n");
      continue;
    }
    do { clear_option = getchar(); } while (clear_option != '\n');
    next_option -= 48;

    printf("#==========================#\n");
    switch (next_option) {
      case 0:
        *rom_title = 0;
        read_header(fd, 1);
        break;
      case 1:
        *rom_title = 0;
        read_header(fd, verbose);
        read_rom(fd);
        break;
      case 2:
        *rom_title = 0;
        read_header(fd, verbose);
        read_ram(fd);
        break;
      case 3:
        *rom_title = 0;
        read_header(fd, verbose);
        write_ram(fd);
        break;
      case 4:
        ctrlc = 1;
      default:
        break;
    }
  } while (!ctrlc);

  if (verbose && ctrlc) printf("ABORTED OK!\n");

#if defined(_WIN32) || defined(_WIN64)
  CloseHandle(fd);

#else
  /* Revert to original port config */
  if (tcsetattr(fd, TCSANOW, &port_attr_orig) == -1) {
    printf("Error reverting original port config: %s\n", strerror(errno));
  }

  close(fd);

#endif /* _WIN32 || _WIN64 */

  return EXIT_SUCCESS;
}
