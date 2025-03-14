#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MULTICAST_ADDR "239.0.0.1"
#define PORT 12345
#define BUFFER_SIZE 4096

int main() {
    int sock;
    struct sockaddr_in multicast_addr;
    char buffer[BUFFER_SIZE];
    FILE *wav_file;

    // Create socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Failed to create socket");
        exit(1);
    }

    // Set up multicast address
    memset(&multicast_addr, 0, sizeof(multicast_addr));
    multicast_addr.sin_family = AF_INET;
    multicast_addr.sin_addr.s_addr = inet_addr(MULTICAST_ADDR);
    multicast_addr.sin_port = htons(PORT);

    // Open WAV file
    wav_file = fopen("input.wav", "rb");
    if (wav_file == NULL) {
        perror("Failed to open WAV file");
        exit(1);
    }

    // Send WAV header
    fread(buffer, 1, 44, wav_file);  // Assuming standard 44-byte WAV header
    sendto(sock, buffer, 44, 0, (struct sockaddr*)&multicast_addr, sizeof(multicast_addr));
    
    printf("Sent WAV header\n");
    sleep(1);  // Give receiver time to process header

    // Send audio data
    while (!feof(wav_file)) {
        size_t bytes_read = fread(buffer, 1, BUFFER_SIZE, wav_file);
        if (bytes_read > 0) {
            sendto(sock, buffer, bytes_read, 0, (struct sockaddr*)&multicast_addr, sizeof(multicast_addr));
            printf("Sent %zu bytes\n", bytes_read);
            usleep(50000);  // Add a small delay to prevent overwhelming the network
        }
    }

    fclose(wav_file);
    close(sock);

    return 0;
}