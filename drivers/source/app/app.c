#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>

#define ON_BOTAO 1
#define ON_LEDR 2
#define ON_LEDG 3
#define ON_SWITCH 4
#define ON_SSEG 5
#define ON_SSEG2 6

unsigned char hexdigit[] = {0x3F, 0x06, 0x5B, 0x4F,
                            0x66, 0x6D, 0x7D, 0x07, 
                            0x7F, 0x6F, 0x77, 0x7C,
			                      0x39, 0x5E, 0x79, 0x71};

int get(int j) {
  int k = hexdigit[j & 0xF]
      | (hexdigit[(j >>  4) & 0xF] << 8)
      | (hexdigit[(j >>  8) & 0xF] << 16)
      | (hexdigit[(j >> 12) & 0xF] << 24);
  k = ~k;
  return k;
}

void bin(int x) {
  int i;
  for (i = 0; i < 32; i++) {
    printf("%d", x & 1);
    x >>= 1;
  }
  printf("\n");
}

void sleep_ms(int milliseconds) {
#ifdef WIN32
    sleep(milliseconds);
#elif _POSIX_C_SOURCE >= 199309L
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
#else     
    usleep(milliseconds * 1000);
#endif
}

int main() {
  int i, j, k;

  int dev = open("/dev/de2i150_altera", O_RDWR);
  k = 54;
  int byte2 = 1, byte1 = (1 << 17), byte = 1, status = 0;
  int ans = 0, cur = get(50);
  write(dev, &cur, ON_SSEG);
  int status_s_seg = 0;
  for (i = 0; i < 1e5; i++) {
    // read(dev, &status, ON_BOTAO);
    // bin(status);
    byte = get(i);
    write(dev, &byte, ON_SSEG2);
    sleep(1);

    // write(dev, &byte, ON_LEDR);
    // byte1 >>= 1;
    // byte2 <<= 1;
    // if (byte1 == 0) byte1 = (1 << 17);
    // if (byte2 == (1 << 18)) byte2 = 1;
    // byte = byte1 + byte2;
    // sleep_ms(100);
  }
  for (i = 0; i < 1e5; i--) {
    read(dev, &status, 4);
    printf("status is %d\n", status);
    status_s_seg = get(status);
    write(dev, &byte, ON_LEDR);
    byte <<= 1;
    if (byte == (1 << 18)) byte = 1;
    write(dev, &status_s_seg, ON_SSEG);
    sleep(1);
  }

  close(dev);
  
  return 0;
}

