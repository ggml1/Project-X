#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main() {
    unsigned char byte, dummy;
    FILE *LEDPORT;

    // opening the device
    LEDPORT = fopen("/dev/leddevice", "w");
    
    // removing the buffer from the file i/o
    setvbuf(LEDPORT, &dummy, _IONBF, 1);

    byte = 1;

    while (1) {
        // Writing to LED PORT to turn on a LED
        printf("Byte valueis %d\n", byte);
        fwrite(&byte, 1, 1, LEDPORT);
        sleep(1);

        byte <<= 1;
        if (byte == 0) byte = 1; // when overflow occurs
    }

    fclose(LEDPORT);

    return 0;
}
