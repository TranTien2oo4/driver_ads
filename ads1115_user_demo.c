#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdint.h>

// Khai báo struct trước khi dùng trong ioctl macro
struct ads1115_data {
    int channel;
    int32_t voltage;
};

#define ADS1115_IOC_MAGIC     'a'
#define ADS1115_GET_VOLTAGE   _IOWR(ADS1115_IOC_MAGIC, 1, struct ads1115_data)

int main(int argc, char *argv[]) { //argument count, argument vecto
    int fd, ret;
    struct ads1115_data data;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <channel 0-3>\n", argv[0]);
        return EXIT_FAILURE;
    }

    data.channel = atoi(argv[1]);
    if (data.channel < 0 || data.channel > 3) {
        fprintf(stderr, "Invalid channel. Must be 0-3.\n"); 
        return EXIT_FAILURE;
    }

    fd = open("/dev/ads11150", O_RDWR);
    if (fd < 0) {
        perror("Failed to open /dev/ads11150");
        return EXIT_FAILURE;
    }

    ret = ioctl(fd, ADS1115_GET_VOLTAGE, &data);
    if (ret < 0) {
        perror("ioctl failed");
        close(fd);
        return EXIT_FAILURE;
    }

    printf("Channel %d voltage: %d mV\n", data.channel, data.voltage);

    close(fd);
    return EXIT_SUCCESS;
}
