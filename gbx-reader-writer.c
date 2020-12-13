/*
 * Program for connecting to an Arduino running the Arduino-GBx-reader-writer
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

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h> 
#include <signal.h>

#else
#include <unistd.h>
#include <termios.h>
#include <getopt.h>

#endif /* _WIN32 || _WIN64 */


///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
#define SERIAL_BAUDRATE   ( 115200 )
#define SERIAL_TIMEOUT    ( 5      ) /* seconds */

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
#define print_state(T,R)   ( printf("\rState: %lu of %lu (%.1f%%)", T - R, T, (((double)T - (double)R) / (double)T) * 100), fflush(stdout) )

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
static int ctrlc = 0;
static unsigned char verbose = 0;

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
static char rom_title[16];

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
void handle_sig(int signum)
{
  ctrlc = 1;
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
#if defined(_WIN32) || defined(_WIN64)
#define wait_ms   Sleep

#define get_time()      ( GetTickCount()                                   )
#define time_valid(S)   ( ((get_time() - S) < (SERIAL_TIMEOUT * 1000)) )

#else

static void wait_ms(unsigned int in_milliseconds) {
  usleep(in_milliseconds * 1000);
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

#else
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

#define FLUSH_PORT(fd)   ( PurgeComm(fd, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR| PURGE_RXCLEAR) )

#else
#define HANDLE   int

#define FLUSH_PORT(fd)   ( tcflush(fd, TCIOFLUSH) )

#endif /* _WIN32 || _WIN64 */

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
  printf("\nUsage: %s [OPTIONS...]\n", program_name);
  printf("\n");
  printf("  -p, --port=/dev/*        arduino serial port.\n");
  printf("  -v, --verbose            print debug.\n");
  printf("  -h, --help               print this screen.\n");
  printf("\nExample:\n");
  printf("  %s -p /dev/ttyACM0\n", program_name);
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
    default:     return("UNKNOW");
  }
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
    default:     return("UNKNOW");
  }
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
    default:     return("UNKNOW");
  }
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
static unsigned char send_routine(HANDLE fd, char CMD)
{
  int i;
  unsigned char tx_packet[7];

  i = 0;
  tx_packet[i++] = 0x10;
  tx_packet[i++] = 0x02;
  tx_packet[i++] = 0x00;
  tx_packet[i++] = 0x00;
  tx_packet[i++] = 0x00;
  tx_packet[i++] = 0x01;
  tx_packet[i++] = CMD;

  if (verbose) printf("send_routine\n");
  if (verbose) print_packet(tx_packet, sizeof(tx_packet));
  if (write(fd, tx_packet, sizeof(tx_packet)) != sizeof(tx_packet)) {
    printf("Error sending command: %s\n", strerror(errno));
    return 1;
  }

  return 0;
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
static ssize_t receive_packet_header_size(HANDLE fd)
{
  ssize_t ret;
  ssize_t available;
  unsigned char c;
  unsigned char has_dle;
  unsigned char has_stx;
  unsigned long start;

  if (verbose) printf("receive_packet_header_size\n");

  /* Wait for response */
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

  if (ctrlc) return -2;
  if (!has_dle || !has_stx) {
    printf("TIMEOUT: DLE and/or STX not received!\n");
    return -3;
  }

  available = -1;
  {
    ssize_t i = 0;
    ssize_t j = 4;
    unsigned char BuffSize[4];
    start = get_time();
    do {
      ret = read(fd, &(BuffSize[i]), j);
      i += ret;
      j -= ret;
    } while ((j > 0) && !ctrlc && time_valid(start));
    if (i == 4) {
      available = ((BuffSize[0] << 24) | (BuffSize[1] << 16) | (BuffSize[2] << 8) | BuffSize[3]);
      if (verbose) printf("Received packet: %d %d %d %d => Total: %lu\n", BuffSize[0], BuffSize[1], BuffSize[2], BuffSize[3], available);
    }
  }

  if (ctrlc) return -2;
  if (available == -1) {
    printf("Error receiving packet size\n");
    return -1;
  }

  return available;
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
static unsigned char receive_routine_buffer(HANDLE fd, ssize_t packet_size, char *out_buff, ssize_t out_buff_size, unsigned char print_state)
{
  ssize_t ret;
  ssize_t offset;
  unsigned long start;
  const ssize_t _packet_size = packet_size;

  if (verbose) printf("receive_routine_buffer\n");

  if (print_state) printf("#==========================#\n");
  offset = 0;
  start = get_time();
  do {
    ret = read(fd, &(out_buff[offset]), packet_size - offset);
    if (ret > 0) {
      offset += ret;
      packet_size -= ret;
      start = get_time();
    }
    else {
      wait_ms(100); /* let's wait */
    }
    if (print_state) print_state(_packet_size, packet_size);

  } while ((packet_size > 0) && !ctrlc && time_valid(start));
  if (print_state) printf("\n");

  if (ctrlc) return 1;
  if (packet_size > 0) {
    printf("ERROR: missing data!!!\n");
    return 1;
  }

  return 0;
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
static unsigned char receive_routine_file(HANDLE fd, ssize_t packet_size, FILE *fp, unsigned char print_state)
{
  ssize_t ret;
  unsigned long start;
  unsigned char rx_chunk[513];
  const ssize_t _packet_size = packet_size;

  if (verbose) printf("receive_routine_file\n");

  if (print_state) printf("#==========================#\n");
  start = get_time();
  do {
    ret = read(fd, rx_chunk, sizeof(rx_chunk) - 1);
    if (ret > 0) {
      if (fwrite(rx_chunk, 1, ret, fp) != ret) {
        printf("Error writing to file: %s\n", strerror(errno));
        return 1;
      }
      packet_size -= ret;
      start = get_time();
    }
    else {
      wait_ms(100); /* let's wait */
    }
    if (print_state) print_state(_packet_size, packet_size);
    
  } while ((packet_size > 0) && !ctrlc && time_valid(start));
  if (print_state) printf("\n");
  
  if (ctrlc) return 1;
  if (packet_size > 0) {
    printf("ERROR: missing data!!!\n");
    return 1;
  }

  return 0;
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
static void read_metadata(HANDLE fd, unsigned char to_print)
{
  ssize_t ret;
  char CartridgeInfo[32];

  if (verbose) printf("read_metadata\n");

  *rom_title = 0;

  FLUSH_PORT(fd);

  if (send_routine(fd, 0x01)) return;

  wait_ms(100); /* give some time */

  ret = receive_packet_header_size(fd);
  if (ret > 0) {
    if (receive_routine_buffer(fd, ret, CartridgeInfo, sizeof(CartridgeInfo), to_print)) return;
    if (to_print) {
      printf("#==========================#\n");
      printf("Rom title: %s\n", CartridgeInfo + 1);
      printf("Cartridge type: %s\n", print_cartridge_string(CartridgeInfo[CartridgeInfo[0] + 1 + 1]));
      printf("Rom size: %s\n", print_rom_size(CartridgeInfo[CartridgeInfo[0] + 1 + 2]));
      printf("Ram size: %s\n", print_ram_size(CartridgeInfo[CartridgeInfo[0] + 1 + 3]));
      printf("Rom version: %d\n", CartridgeInfo[CartridgeInfo[0] + 1 + 4]);
      printf("Checksum: %d\n\n", CartridgeInfo[CartridgeInfo[0] + 1 + 5]);
    }
    memcpy(rom_title, CartridgeInfo + 1, CartridgeInfo[0] + 1);
  }

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

  //if (rom_title[0] == 0) return;

  i = strlen(rom_title);
  memcpy(rom_filename, rom_title, i + 1);
  rom_filename[i++] = '.';
  rom_filename[i++] = 'g';
  rom_filename[i++] = 'b';
  rom_filename[i++] = '\0';

  fp = fopen(rom_filename, "w");
  if (!fp) {
    printf("Error creating %s: %s\n", rom_title, strerror(errno));
    return;
  }

  FLUSH_PORT(fd);

  if (send_routine(fd, 0x02)) return;

  wait_ms(100); /* give some time */

  size = receive_packet_header_size(fd);
  if (size > 0) {
    receive_routine_file(fd, size, fp, 1);
  }

  fclose(fp);

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

  //if (rom_title[0] == 0) return;

  i = strlen(rom_title);
  memcpy(ram_filename, rom_title, i + 1);
  ram_filename[i++] = '.';
  ram_filename[i++] = 's';
  ram_filename[i++] = 'a';
  ram_filename[i++] = 'v';
  ram_filename[i++] = '\0';

  fp = fopen(ram_filename, "w");
  if (!fp) {
    printf("Error creating %s: %s\n", rom_title, strerror(errno));
    return;
  }

  FLUSH_PORT(fd);

  if (send_routine(fd, 0x03)) return;

  wait_ms(100); /* give some time */

  size = receive_packet_header_size(fd);
  if (size > 0) {
    receive_routine_file(fd, size, fp, 1);
  }

  fclose(fp);

}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
int main(int argc, char *argv[])
{
  HANDLE fd;
  int next_option;

#if defined(_WIN32) || defined(_WIN64)
  //TODO
  fd = CreateFile(L"COM9", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
  if (fd != INVALID_HANDLE_VALUE) {
    BOOL Status;
    DCB dcb = { 0 };
    COMMTIMEOUTS tmo = { MAXDWORD, 0, 0, 0, 0 };
    
    memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);
    Status = GetCommState(fd, &dcb);
    if (Status == FALSE) exit(1);
    
    dcb.BaudRate = 400000;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity = NOPARITY;
    //dcb.fAbortOnError = FALSE;
    Status = SetCommState(fd, &dcb);
    if (Status == FALSE) exit(1);
    
    Status = SetCommTimeouts(fd, &tmo);
    if (Status == FALSE) exit(1);

    Status = SetCommMask(fd, EV_RXCHAR | EV_TXEMPTY | EV_ERR);
    if (Status == FALSE) exit(1);
  }
  verbose = 1;
#else
  char *port_name = NULL;

  struct termios tty_attr, tty_attr_orig;
  speed_t i_speed, o_speed;

  extern char *optarg;
  const char* short_options = "p:vh";
  const struct option long_options[] = {
    { "port",         required_argument, NULL, 'p' },
    { "verbose",      no_argument,       NULL, 'v' },
    { "help",         no_argument,       NULL, 'h' },
    { 0,              0,                 0,     0  }
  };

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
        break;
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
    printf("\nSorry, no arduino tty device.\n\n");
    print_usage(argv[0]);
    return EXIT_FAILURE;
  }

  /* Install CTRL^C signal handler */
  sigaction(SIGINT, &int_handler, 0);
  
  /* Open tty */
  fd = open(port_name, O_RDWR);
  if (fd == -1) {
    printf("Error opening %s: %s\n", port_name, strerror(errno));
    return EXIT_FAILURE;
  }
  if (verbose) printf("%s opened.\n", port_name);

  /* Get tty config and make backup */
  tcgetattr(fd, &tty_attr);
  tty_attr_orig = tty_attr;

  /* Configure tty */
  i_speed = cfgetispeed(&tty_attr);
  if (i_speed != SERIAL_BAUDRATE) {
    cfsetispeed(&tty_attr, SERIAL_BAUDRATE);
  }

  o_speed = cfgetospeed(&tty_attr);
  if (o_speed != SERIAL_BAUDRATE) {
    cfsetospeed(&tty_attr, SERIAL_BAUDRATE);
  }

  tty_attr.c_cc[VMIN] = 0; /* Return always (non-block) */
  tty_attr.c_cc[VTIME] = 0; /* Never return from read due to timeout */

  tty_attr.c_iflag &= ~(ICRNL | IXON);
  tty_attr.c_oflag &= ~OPOST;
  tty_attr.c_lflag &= ~(ISIG | ICANON | IEXTEN | ECHO | ECHOE | ECHOK | ECHOCTL | ECHOKE);

  if (tcsetattr(fd, TCSANOW, &tty_attr) == -1) {
    printf("Error while setting %s options: %s\n", port_name, strerror(errno));
    close(fd);
    return EXIT_FAILURE;
  }

  if (verbose) printf("%s successfully configured.\n", port_name);
#endif /* _WIN32 || _WIN64 */

  do {
    printf("#=========================================================#\n");
    printf("#=============== Arduino-GBx-Reader-Writer ===============#\n");
    printf("0) Read Cartidge Header\n");
    printf("1) Read ROM\n");
    printf("2) Read RAM\n");
    printf("3) WRITE RAM\n");
    printf("4) EXIT\n");
    printf("Select an option: ");
    next_option = getchar();
    if ((next_option < 48) || (next_option > 57)) {
      printf("Invalid option\n");
      continue;
    }
    next_option -= 48;
    getchar(); /* clear '\n' */

    switch (next_option) {
      case 0:
        *rom_title = 0;
        read_metadata(fd, 1);
        break;
      case 1:
        read_metadata(fd, 0);
        wait_ms(50); /* give some time */
        read_rom(fd);
        break;
      case 2:
        read_metadata(fd, 0);
        wait_ms(50); /* give some time */
        read_ram(fd);
        break;
      case 3:
        printf("TODO\n");
        break;
      case 4:
        ctrlc = 1;
      default:
        break;
    }
  } while (!ctrlc);

#if defined(_WIN32) || defined(_WIN64)
  CloseHandle(fd);
#else
  /* Revert to original tty config */
  if (tcsetattr(fd, TCSANOW, &tty_attr_orig) == -1) {
    printf("Error while reverting original tty config: %s\n", strerror(errno));
  }

  close(fd);
#endif /* _WIN32 || _WIN64 */

  return EXIT_SUCCESS;
}
