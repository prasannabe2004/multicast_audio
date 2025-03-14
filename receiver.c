#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <alsa/asoundlib.h>
#include <endian.h>

#define MULTICAST_ADDR "239.0.0.1"
#define PORT 12345
#define BUFFER_SIZE 4096

typedef struct {
    char chunk_id[4];
    uint32_t chunk_size;
    char format[4];
    char subchunk1_id[4];
    uint32_t subchunk1_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
} __attribute__((packed)) WavHeader;

void print_hex(const char* data, int length) {
    for (int i = 0; i < length; i++) {
        printf("%02X ", (unsigned char)data[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    printf("\n");
}

int main() {
    int sock;
    struct sockaddr_in multicast_addr;
    struct ip_mreq mreq;
    char buffer[BUFFER_SIZE];
    snd_pcm_t *pcm_handle;
    snd_pcm_hw_params_t *params;
    int rc;

    // Create socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Failed to create socket");
        exit(1);
    }

    // Set up multicast address
    memset(&multicast_addr, 0, sizeof(multicast_addr));
    multicast_addr.sin_family = AF_INET;
    multicast_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    multicast_addr.sin_port = htons(PORT);

    // Bind socket
    if (bind(sock, (struct sockaddr*)&multicast_addr, sizeof(multicast_addr)) < 0) {
        perror("Failed to bind socket");
        exit(1);
    }

    // Join multicast group
    mreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_ADDR);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        perror("Failed to join multicast group");
        exit(1);
    }

    printf("Waiting for WAV header...\n");

    // Receive the entire WAV header
    WavHeader header;
    int bytes_received = recv(sock, &header, sizeof(WavHeader), 0);
    if (bytes_received < sizeof(WavHeader)) {
        fprintf(stderr, "Failed to receive complete WAV header. Received %d bytes.\n", bytes_received);
        print_hex((char*)&header, bytes_received);
        exit(1);
    }

    printf("Received %d bytes of WAV header:\n", bytes_received);
    print_hex((char*)&header, sizeof(WavHeader));

    // Validate header
    if (memcmp(header.chunk_id, "RIFF", 4) != 0 || memcmp(header.format, "WAVE", 4) != 0) {
        fprintf(stderr, "Invalid WAV header\n");
        exit(1);
    }

    printf("WAV header parsed successfully\n");
    printf("Sample rate: %u\n", le32toh(header.sample_rate));
    printf("Channels: %u\n", le16toh(header.num_channels));
    printf("Bits per sample: %u\n", le16toh(header.bits_per_sample));

    // Set up ALSA
    rc = snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (rc < 0) {
        fprintf(stderr, "Unable to open PCM device: %s\n", snd_strerror(rc));
        exit(1);
    }

    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(pcm_handle, params);

    rc = snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (rc < 0) {
        fprintf(stderr, "Error setting access: %s\n", snd_strerror(rc));
        exit(1);
    }

    rc = snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_S16_LE);
    if (rc < 0) {
        fprintf(stderr, "Error setting format: %s\n", snd_strerror(rc));
        exit(1);
    }

    rc = snd_pcm_hw_params_set_channels(pcm_handle, params, le16toh(header.num_channels));
    if (rc < 0) {
        fprintf(stderr, "Error setting channels: %s\n", snd_strerror(rc));
        exit(1);
    }

    rc = snd_pcm_hw_params_set_rate(pcm_handle, params, le32toh(header.sample_rate), 0);
    if (rc < 0) {
        fprintf(stderr, "Error setting rate: %s\n", snd_strerror(rc));
        exit(1);
    }

    rc = snd_pcm_hw_params(pcm_handle, params);
    if (rc < 0) {
        fprintf(stderr, "Error setting HW params: %s\n", snd_strerror(rc));
        exit(1);
    }

    printf("Starting audio playback...\n");

    int total_bytes_received = 0;
    int packet_count = 0;

    // Receive and play audio data
    while (1) {
        bytes_received = recv(sock, buffer, BUFFER_SIZE, 0);
        if (bytes_received > 0) {
            total_bytes_received += bytes_received;
            packet_count++;
            printf("Received packet %d, %d bytes (total: %d bytes)\n", packet_count, bytes_received, total_bytes_received);

            int bytes_per_sample = le16toh(header.bits_per_sample) / 8;
            int frames = bytes_received / bytes_per_sample / le16toh(header.num_channels);

            rc = snd_pcm_writei(pcm_handle, buffer, frames);
            if (rc == -EPIPE) {
                fprintf(stderr, "Underrun occurred\n");
                snd_pcm_prepare(pcm_handle);
            } else if (rc < 0) {
                fprintf(stderr, "Error from writei: %s\n", snd_strerror(rc));
            } else if (rc != frames) {
                fprintf(stderr, "Short write (expected %d, wrote %d)\n", frames, rc);
            }
        }
    }

    snd_pcm_drain(pcm_handle);
    snd_pcm_close(pcm_handle);
    close(sock);

    return 0;
}