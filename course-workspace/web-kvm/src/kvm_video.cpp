/*
 * T527 Video Capture + Hardware Encode
 * Modified for luckfox_kvm - Unix Socket output
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <errno.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdarg.h>
#include <sys/mman.h>

#include "AWVideoEncoder.h"
#include "sunxiMemInterface.h"

#define VIDEO_VERSION     "1.0.0"
#define VIDEO_DEV         "/dev/video0"
#define WIDTH            1920
#define HEIGHT           1080
#define FPS              60
#define BITRATE          8000
#define BUFFER_COUNT     3

// Unix Socket for video data
#define VIDEO_SOCKET_PATH  "/tmp/kvm_video_stream.sock"
#define CTRL_SOCKET      "/var/run/kvm_ctrl.sock"

#define LOG_FILE         "/tmp/kvm_video.log"

#define KVM2_HEADER_SIZE 20
#define KVM2_MAX_PAYLOAD 4147200

static void put_le32(uint8_t* out, uint32_t value)
{
    out[0] = value & 0xff; out[1] = (value >> 8) & 0xff;
    out[2] = (value >> 16) & 0xff; out[3] = (value >> 24) & 0xff;
}

static void put_le64(uint8_t* out, uint64_t value)
{
    put_le32(out, (uint32_t)value);
    put_le32(out + 4, (uint32_t)(value >> 32));
}

static int write_all(int fd, const uint8_t* data, size_t length)
{
    size_t offset = 0;
    while (offset < length) {
        ssize_t written = write(fd, data + offset, length - offset);
        if (written > 0) { offset += (size_t)written; continue; }
        if (written < 0 && errno == EINTR) continue;
        return -1;
    }
    return 0;
}

// Helper for writing logs to file
static void log_to_file(const char* format, ...)
{
    FILE* f = fopen(LOG_FILE, "a");
    if (!f) return;
    
    // Add timestamp
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm* tm_info = localtime(&tv.tv_sec);
    char time_buf[26];
    strftime(time_buf, 26, "%Y-%m-%d %H:%M:%S", tm_info);
    fprintf(f, "[%s.%03d] ", time_buf, (int)(tv.tv_usec / 1000));
    
    va_list args;
    va_start(args, format);
    vfprintf(f, format, args);
    va_end(args);
    
    fclose(f);
}

// Custom printf that writes to both stdout and file
#define kvm_log(fmt, args...) do { \
    fprintf(stdout, fmt, ##args); \
    log_to_file(fmt, ##args); \
} while(0)

#define ALIGN_16B(x) (((x) + (15)) & ~(15))

using namespace awvideoencoder;

// Global variables
static int g_video_fd = -1;
static bool g_running = false;
static bool g_capture_started = false;
static bool g_capture_stopping = false;
static time_t g_last_capture_stop_time = 0;
static pthread_t g_cap_thread = 0;
static AWVideoEncoder* g_encoder = NULL;

// Ion memory for encoder input
static dma_mem_des_t gIonMem;
static bool g_ion_inited = false;

// Socket fds
static int g_ctrl_fd = -1;

// Mutex for thread safety
static pthread_mutex_t g_cap_mutex = PTHREAD_MUTEX_INITIALIZER;

// V4L2 buffer structure
typedef struct {
    void* start[3];
    int length[3];
    void* phy_addr[3];
    unsigned int fd[3];
} buffer_t;

static buffer_t* g_buffers = NULL;
static int g_buffer_count = 0;

// Socket file descriptor for video output
static int g_video_socket_fd = -1;

// Timestamp for frames
static uint64_t g_encoder_start_time = 0;

// Get current timestamp in milliseconds
static uint64_t get_timestamp_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

// Encode uint64 to big-endian bytes
static void encode_uint64(uint8_t* buf, uint64_t val)
{
    buf[0] = (uint8_t)((val >> 56) & 0xFF);
    buf[1] = (uint8_t)((val >> 48) & 0xFF);
    buf[2] = (uint8_t)((val >> 40) & 0xFF);
    buf[3] = (uint8_t)((val >> 32) & 0xFF);
    buf[4] = (uint8_t)((val >> 24) & 0xFF);
    buf[5] = (uint8_t)((val >> 16) & 0xFF);
    buf[6] = (uint8_t)((val >> 8) & 0xFF);
    buf[7] = (uint8_t)(val & 0xFF);
}

// Encode uint32 to big-endian bytes
static void encode_uint32(uint8_t* buf, uint32_t val)
{
    buf[0] = (uint8_t)((val >> 24) & 0xFF);
    buf[1] = (uint8_t)((val >> 16) & 0xFF);
    buf[2] = (uint8_t)((val >> 8) & 0xFF);
    buf[3] = (uint8_t)(val & 0xFF);
}

// Helper: Find all NAL units in encoder output buffer
// Returns total size of all NAL units found
static int findAllNALUnits(const uint8_t* data, int maxLen, int* nalStarts, int* nalEnds, int maxNals)
{
    int nalCount = 0;
    int i = 0;

    while (i < maxLen - 4 && nalCount < maxNals) {
        // Check for 4-byte start code (0x00 0x00 0x00 0x01)
        if (data[i] == 0x00 && data[i+1] == 0x00 && data[i+2] == 0x00 && data[i+3] == 0x01) {
            nalStarts[nalCount] = i + 4;  // Skip start code, point to NAL header
            if (nalCount > 0) {
                nalEnds[nalCount - 1] = i;  // Previous NAL ends here
            }
            nalCount++;
            i += 4;
            continue;
        }
        // Check for 3-byte start code (0x00 0x00 0x01)
        if (data[i] == 0x00 && data[i+1] == 0x00 && data[i+2] == 0x01) {
            nalStarts[nalCount] = i + 3;  // Skip start code, point to NAL header
            if (nalCount > 0) {
                nalEnds[nalCount - 1] = i;  // Previous NAL ends here
            }
            nalCount++;
            i += 3;
            continue;
        }
        i++;
    }

    // Set end of last NAL
    if (nalCount > 0) {
        nalEnds[nalCount - 1] = maxLen;
    }

    return nalCount;
}

// Encoder callback - directly output Annex-B H.264 data
// Simplified version: send raw buffer directly to backend without NAL parsing
class EncoderCallback : public AWVideoEncoderDataCallback {
public:
    EncoderCallback() {}
    virtual ~EncoderCallback() {}

    virtual int encoderDataReady(AVPacket* packet) override
    {
        static int encode_count = 0;

        if (!packet || packet->dataLen0 <= 0 || !packet->pAddrVir0) {
            kvm_log("[ENCODER] ERROR: invalid packet (packet=%p, dataLen0=%d, pAddrVir0=%p)\n",
                   packet, packet ? packet->dataLen0 : -1, packet ? packet->pAddrVir0 : NULL);
            return -1;
        }

        // Initialize start time on first frame
        if (g_encoder_start_time == 0) {
            g_encoder_start_time = get_timestamp_ms();
            kvm_log("[ENCODER] START: encoder started at ts=%llu\n", (unsigned long long)g_encoder_start_time);
        }

        uint64_t timestamp = get_timestamp_ms() - g_encoder_start_time;

        // Raw buffer from encoder
        const uint8_t* data0 = (const uint8_t*)packet->pAddrVir0;
        int dataLen0 = packet->dataLen0;

        const uint8_t* data1 = (const uint8_t*)packet->pAddrVir1;
        int dataLen1 = packet->dataLen1;

        int totalLen = dataLen0 + dataLen1;

        // Debug log for first 5 frames
        if (encode_count < 5) {
            kvm_log("[ENCODER] frame %d: dataLen0=%d, dataLen1=%d, totalLen=%d\n",
                   encode_count, dataLen0, dataLen1, totalLen);
        }

        // Send RAW buffer to Unix Socket
        // Format: [4B LE length][H.264 data]
        static int socket_was_closed = 0;
        static int socket_error_shown = 0;
        if (g_video_socket_fd >= 0) {
            if (socket_was_closed) {
                kvm_log("[ENCODER] Socket connected again, resuming video transmission\n");
                socket_was_closed = 0;
                socket_error_shown = 0;
            }

            if (totalLen <= 0 || totalLen > KVM2_MAX_PAYLOAD) {
                kvm_log("[ENCODER] Invalid KVM2 payload length: %d\n", totalLen);
                return -1;
            }
            uint8_t header[KVM2_HEADER_SIZE] = {'K', 'V', 'M', '2'};
            put_le32(header + 4, (uint32_t)totalLen);
            put_le64(header + 8, timestamp * 1000ULL);
            put_le32(header + 16, 0);
            if (write_all(g_video_socket_fd, header, sizeof(header)) < 0) {
                kvm_log("[ENCODER] Socket write error: errno=%d\n", errno);
                close(g_video_socket_fd);
                g_video_socket_fd = -1;
                socket_was_closed = 1;
                return 0;
            }

            // Write raw H.264 data part 0
            if (dataLen0 > 0 && data0 != NULL) {
                if (write_all(g_video_socket_fd, data0, (size_t)dataLen0) < 0) {
                    kvm_log("[ENCODER] Socket data write error: errno=%d\n", errno);
                    close(g_video_socket_fd);
                    g_video_socket_fd = -1;
                    socket_was_closed = 1;
                    return 0;
                }
            }

            // Write raw H.264 data part 1
            if (dataLen1 > 0 && data1 != NULL) {
                if (write_all(g_video_socket_fd, data1, (size_t)dataLen1) < 0) {
                    kvm_log("[ENCODER] Socket data1 write error: errno=%d\n", errno);
                    close(g_video_socket_fd);
                    g_video_socket_fd = -1;
                    socket_was_closed = 1;
                    return 0;
                }
            }

            if (encode_count < 5) {
                kvm_log("[ENCODER] Socket OK: sent %d bytes\n", totalLen);
            }
        } else {
            socket_was_closed = 1;
            if (socket_error_shown == 0) {
                kvm_log("[ENCODER] Socket not open, dropping frame\n");
                socket_error_shown = 1;
            }
        }

        encode_count++;
        return 0;
    }
};

static EncoderCallback* g_callback = NULL;

// Signal handler
static void signal_handler(int sig)
{
    kvm_log("\n[INFO] Received signal %d, stopping...\n", sig);
    g_running = false;

    if (g_ctrl_fd >= 0) {
        shutdown(g_ctrl_fd, SHUT_RDWR);
    }
}

// Connect to luckfox_kvm ctrl socket with retry, connect video Unix Socket
// Note: Socket (video data) is critical - encoder cannot start without it
static int connect_sockets(void)
{
    int retry_count = 0;
    while (retry_count < 120 && g_running) {
        // Connect to ctrl socket
        g_ctrl_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (g_ctrl_fd >= 0) {
            struct sockaddr_un addr;
            memset(&addr, 0, sizeof(addr));
            addr.sun_family = AF_UNIX;
            strncpy(addr.sun_path, CTRL_SOCKET, sizeof(addr.sun_path) - 1);
            if (connect(g_ctrl_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                close(g_ctrl_fd);
                g_ctrl_fd = -1;
            }
        }

        // Connect to video Unix Socket - CRITICAL for video transmission
        if (g_video_socket_fd < 0) {
            g_video_socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
            if (g_video_socket_fd >= 0) {
                struct sockaddr_un addr;
                memset(&addr, 0, sizeof(addr));
                addr.sun_family = AF_UNIX;
                strncpy(addr.sun_path, VIDEO_SOCKET_PATH, sizeof(addr.sun_path) - 1);
                if (connect(g_video_socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                    close(g_video_socket_fd);
                    g_video_socket_fd = -1;
                }
            }
            if (g_video_socket_fd >= 0) {
                kvm_log("[INFO] Connected video socket: %s (fd=%d)\n", VIDEO_SOCKET_PATH, g_video_socket_fd);
            } else {
                kvm_log("[WARN] Failed to connect video socket: %s (errno=%d), retrying...\n", VIDEO_SOCKET_PATH, errno);
            }
        }

        if (g_ctrl_fd >= 0 && g_video_socket_fd >= 0) {
            kvm_log("[INFO] CONNECTED: ctrl_fd=%d, video_socket_fd=%d\n", g_ctrl_fd, g_video_socket_fd);
            return 0;
        }

        // Close and retry
        if (g_ctrl_fd >= 0) { close(g_ctrl_fd); g_ctrl_fd = -1; }
        if (g_video_socket_fd >= 0) { close(g_video_socket_fd); g_video_socket_fd = -1; }

        kvm_log("[INFO] Waiting for luckfox_kvm... retry %d/120\n", retry_count + 1);
        sleep(2);
        retry_count++;
    }

    kvm_log("[INFO] Socket connections: ctrl=%d, video_socket=%d\n", g_ctrl_fd, g_video_socket_fd);
    return (g_ctrl_fd >= 0 && g_video_socket_fd >= 0) ? 0 : -1;
}

// Send video state to ctrl socket
static void send_video_state(int width, int height, float fps)
{
    if (g_ctrl_fd < 0) return;

    char json[256];
    snprintf(json, sizeof(json),
        "{\"event\":\"video_input_state\",\"data\":{\"ready\":true,\"width\":%d,\"height\":%d,\"fps\":%.1f}}\n",
        width, height, fps);

    send(g_ctrl_fd, json, strlen(json), 0);
    kvm_log("[INFO] Sent video state: %dx%d @ %.1ffps\n", width, height, fps);
}

// Init Ion memory
static int init_ion_memory(void)
{
    int ret;
    int frame_size = ALIGN_16B(WIDTH) * ALIGN_16B(HEIGHT) * 3 / 2;

    ret = allocOpen(MEM_TYPE_CDX_NEW, &gIonMem, NULL);
    if (ret < 0) {
        kvm_log("[ERROR] allocOpen failed\n");
        return -1;
    }

    gIonMem.size = frame_size;
    ret = allocAlloc(MEM_TYPE_CDX_NEW, &gIonMem, NULL);
    if (ret < 0) {
        kvm_log("[ERROR] allocAlloc failed\n");
        allocClose(MEM_TYPE_CDX_NEW, &gIonMem, NULL);
        return -1;
    }

    kvm_log("[INFO] Ion memory allocated: vir=%p, phy=0x%lx, size=%d\n",
           gIonMem.vir, gIonMem.phy, gIonMem.size);

    g_ion_inited = true;
    return 0;
}

// Free Ion memory
static void free_ion_memory(void)
{
    if (!g_ion_inited) return;

    allocFree(MEM_TYPE_CDX_NEW, &gIonMem, NULL);
    allocClose(MEM_TYPE_CDX_NEW, &gIonMem, NULL);
    g_ion_inited = false;
    kvm_log("[INFO] Ion memory freed\n");
}

// Forward declaration
static void* capture_thread(void* arg);

// Try to connect to ctrl socket only (for reconnection)
static int connect_ctrl_socket(void)
{
    if (g_ctrl_fd >= 0) {
        return 0;
    }

    g_ctrl_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (g_ctrl_fd < 0) {
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, CTRL_SOCKET, sizeof(addr.sun_path) - 1);

    if (connect(g_ctrl_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(g_ctrl_fd);
        g_ctrl_fd = -1;
        return -1;
    }

    kvm_log("[CTRL] Reconnected to ctrl socket\n");
    return 0;
}

// Simple JSON parsing for control commands
static int parse_action(const char* json_str, char* action, int max_len)
{
    const char* p = strstr(json_str, "\"action\"");
    if (!p) return -1;

    p = strchr(p, ':');
    if (!p) return -1;
    p++;

    while (*p == ' ' || *p == '\t') p++;
    if (*p == '\"') p++;

    int i = 0;
    while (*p && *p != '\"' && i < max_len - 1) {
        action[i++] = *p++;
    }
    action[i] = '\0';

    return 0;
}

// Control command processing thread
static void* control_thread(void* arg)
{
    char buffer[512];

    kvm_log("[CTRL] Control thread started, waiting for commands...\n");

    while (g_running) {
        // Try to reconnect ctrl socket if disconnected
        if (g_ctrl_fd < 0) {
            if (connect_ctrl_socket() < 0) {
                sleep(1);
                continue;
            }
        }

        // Try to reconnect socket if disconnected (important!)
        if (g_video_socket_fd < 0) {
            g_video_socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
            if (g_video_socket_fd >= 0) {
                struct sockaddr_un addr;
                memset(&addr, 0, sizeof(addr));
                addr.sun_family = AF_UNIX;
                strncpy(addr.sun_path, VIDEO_SOCKET_PATH, sizeof(addr.sun_path) - 1);
                if (connect(g_video_socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                    close(g_video_socket_fd);
                    g_video_socket_fd = -1;
                }
            }
            if (g_video_socket_fd >= 0) {
                kvm_log("[CTRL] Socket reopened: fd=%d\n", g_video_socket_fd);
            }
        }

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(g_ctrl_fd, &fds);

        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int ret = select(g_ctrl_fd + 1, &fds, NULL, NULL, &tv);
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (ret == 0) continue;

        int n = read(g_ctrl_fd, buffer, sizeof(buffer) - 1);
        if (n <= 0) {
            kvm_log("[CTRL] Ctrl socket closed, reconnecting...\n");
            close(g_ctrl_fd);
            g_ctrl_fd = -1;
            continue;
        }

        buffer[n] = '\0';
        kvm_log("[CTRL] Received command: %s\n", buffer);

        char action[64];
        if (parse_action(buffer, action, sizeof(action)) == 0) {
            pthread_mutex_lock(&g_cap_mutex);

            if (strcmp(action, "start_video") == 0) {
                kvm_log("[CTRL] CMD: start_video received\n");
                
                // If we are currently stopping, we must wait for it to finish completely
                // before trying to start again. We check this while holding the mutex.
                while (g_capture_stopping) {
                    kvm_log("[CTRL] Capture is still stopping, waiting for it to finish...\n");
                    // Release mutex, wait a bit, then re-acquire and check again
                    pthread_mutex_unlock(&g_cap_mutex);
                    usleep(200000); // 200ms
                    pthread_mutex_lock(&g_cap_mutex);
                }
                
                // Now we are guaranteed that any previous stop_video has completely finished
                // (including the 1-second V4L2 release delay).

                // Re-connect socket if it was closed
                if (g_video_socket_fd < 0) {
                    kvm_log("[CTRL] Re-connecting video socket...\n");
                    g_video_socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
                    if (g_video_socket_fd >= 0) {
                        struct sockaddr_un addr;
                        memset(&addr, 0, sizeof(addr));
                        addr.sun_family = AF_UNIX;
                        strncpy(addr.sun_path, VIDEO_SOCKET_PATH, sizeof(addr.sun_path) - 1);
                        if (connect(g_video_socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                            close(g_video_socket_fd);
                            g_video_socket_fd = -1;
                            kvm_log("[WARN] Failed to reconnect video socket: %s (errno=%d)\n", VIDEO_SOCKET_PATH, errno);
                        }
                    }
                }
                
                if (!g_capture_started) {
                    kvm_log("[CTRL] Starting capture...\n");
                    g_capture_started = true;
                    pthread_create(&g_cap_thread, NULL, capture_thread, NULL);
                    kvm_log("[CTRL] Capture thread started\n");
                } else {
                    kvm_log("[CTRL] Capture already running\n");
                }
            } else if (strcmp(action, "stop_video") == 0) {
                kvm_log("[CTRL] CMD: stop_video received\n");
                if (g_capture_started) {
                    kvm_log("[CTRL] Stopping capture...\n");
                    g_capture_stopping = true;
                    g_capture_started = false;
                    
                    if (g_cap_thread) {
                        kvm_log("[CTRL] Waiting for capture thread to join...\n");
                        pthread_mutex_unlock(&g_cap_mutex);
                        pthread_join(g_cap_thread, NULL);
                        pthread_mutex_lock(&g_cap_mutex);
                        g_cap_thread = 0;
                        kvm_log("[CTRL] Capture thread joined\n");
                    }
                    // Wait a bit to ensure V4L2 device is completely released by kernel
                    // Moving sleep OUTSIDE the mutex to avoid blocking start_video
                    kvm_log("[CTRL] Sleeping 1s outside mutex to release V4L2...\n");
                    pthread_mutex_unlock(&g_cap_mutex);
                    sleep(1);
                    pthread_mutex_lock(&g_cap_mutex);
                    
                    kvm_log("[CTRL] Sleep 1s done, V4L2 should be released now\n");
                    g_last_capture_stop_time = time(NULL);
                    g_capture_stopping = false;
                    kvm_log("[CTRL] Capture stopped\n");
                } else {
                    kvm_log("[CTRL] Capture not running\n");
                }

                // Close socket when stopping video
                if (g_video_socket_fd >= 0) {
                    kvm_log("[CTRL] Closing video socket...\n");
                    close(g_video_socket_fd);
                    g_video_socket_fd = -1;
                }
            } else {
                kvm_log("[CTRL] Unknown action: %s\n", action);
            }

            pthread_mutex_unlock(&g_cap_mutex);
        }
    }

    kvm_log("[CTRL] Control thread exiting\n");
    return NULL;
}

// Init encoder
static int init_encoder(void)
{
    EncodeParam encParam;
    memset(&encParam, 0, sizeof(EncodeParam));

    encParam.codecType = CODEC_H264;
    encParam.pixelFormat = PIXEL_YUV420SP;
    encParam.srcW = WIDTH;
    encParam.srcH = HEIGHT;
    encParam.dstW = WIDTH;
    encParam.dstH = HEIGHT;
    encParam.rotation = Angle_0;
    encParam.bitRate = BITRATE * 1000;
    encParam.frameRate = FPS;
    encParam.maxKeyFrame = 60;  // IDR every 1 second (参考项目)
    encParam.frameCount = MULTI_FRAMES;
    encParam.rcMode = VBR;
    encParam.minQp = 20;
    encParam.maxQp = 28;

    g_encoder = AWVideoEncoder::create();
    if (!g_encoder) {
        kvm_log("[ERROR] Failed to create encoder\n");
        return -1;
    }

    g_callback = new EncoderCallback();

    int ret = g_encoder->init(&encParam, g_callback);
    if (ret != 0) {
        kvm_log("[ERROR] Failed to init encoder: %d\n", ret);
        AWVideoEncoder::destroy(g_encoder);
        g_encoder = NULL;
        return -1;
    }

    kvm_log("[INFO] Encoder initialized: %dx%d @ %dfps, bitrate=%dkbps\n",
           WIDTH, HEIGHT, FPS, BITRATE);
    kvm_log("[INFO] Encoder config: maxKeyFrame=%d, frameCount=%d, rcMode=%d\n",
           FPS * 2, MULTI_FRAMES, VBR);

    return 0;
}

// Video capture thread
static void* capture_thread(void* arg)
{
    kvm_log("[INFO] Capture thread started\n");

    struct v4l2_format fmt;
    struct v4l2_buffer buf;
    struct v4l2_requestbuffers req;
    struct v4l2_plane planes[BUFFER_COUNT];
    enum v4l2_buf_type type;

    // Open device
    g_video_fd = open(VIDEO_DEV, O_RDWR | O_NONBLOCK);
    if (g_video_fd < 0) {
        kvm_log("[ERROR] Cannot open %s: %s\n", VIDEO_DEV, strerror(errno));
        // Reset capture state so it can be retried later
        g_capture_started = false;
        return NULL;
    }

    // Set input
    struct v4l2_input input;
    input.index = 0;
    if (ioctl(g_video_fd, VIDIOC_S_INPUT, &input) < 0) {
        kvm_log("[ERROR] Cannot set input: %s\n", strerror(errno));
        close(g_video_fd);
        g_video_fd = -1;
        return NULL;
    }

    // Set format
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    fmt.fmt.pix_mp.width = WIDTH;
    fmt.fmt.pix_mp.height = HEIGHT;
    fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12;
    fmt.fmt.pix_mp.field = V4L2_FIELD_NONE;
    ioctl(g_video_fd, VIDIOC_S_FMT, &fmt);

    // Set framerate
    struct v4l2_streamparm parms;
    memset(&parms, 0, sizeof(parms));
    parms.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    parms.parm.capture.timeperframe.numerator = 1;
    parms.parm.capture.timeperframe.denominator = FPS;
    parms.parm.capture.capturemode = 0x0002;
    ioctl(g_video_fd, VIDIOC_S_PARM, &parms);

    // Verify format
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    ioctl(g_video_fd, VIDIOC_G_FMT, &fmt);
    kvm_log("[INFO] Format set: %dx%d\n", fmt.fmt.pix_mp.width, fmt.fmt.pix_mp.height);

    // Send video state to luckfox_kvm
    send_video_state(fmt.fmt.pix_mp.width, fmt.fmt.pix_mp.height, (float)FPS);

    // Request MMAP buffers
    memset(&req, 0, sizeof(req));
    req.count = BUFFER_COUNT;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(g_video_fd, VIDIOC_REQBUFS, &req) < 0) {
        kvm_log("[ERROR] Cannot request buffers: %s\n", strerror(errno));
        close(g_video_fd);
        g_video_fd = -1;
        return NULL;
    }

    g_buffer_count = req.count;
    g_buffers = (buffer_t*)calloc(g_buffer_count, sizeof(buffer_t));

    kvm_log("[INFO] Allocated %d buffers (MMAP mode)\n", g_buffer_count);

    // Map buffers
    for (int i = 0; i < g_buffer_count; i++) {
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        buf.length = 1;
        memset(&planes[0], 0, sizeof(planes[0]));
        buf.m.planes = &planes[0];

        if (ioctl(g_video_fd, VIDIOC_QUERYBUF, &buf) < 0) {
            kvm_log("[ERROR] Cannot query buffer %d\n", i);
            continue;
        }

        g_buffers[i].start[0] = mmap(NULL, planes[0].length, PROT_READ | PROT_WRITE,
                                       MAP_SHARED, g_video_fd, planes[0].m.mem_offset);
        if (g_buffers[i].start[0] == MAP_FAILED) {
            kvm_log("[ERROR] mmap failed for buffer %d\n", i);
            g_buffers[i].start[0] = NULL;
            continue;
        }

        g_buffers[i].length[0] = planes[0].length;
        kvm_log("[INFO] Buffer %d mapped: %d bytes\n", i, planes[0].length);

        // QBUF
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        buf.length = 1;
        buf.m.planes = &planes[0];

        if (ioctl(g_video_fd, VIDIOC_QBUF, &buf) < 0) {
            kvm_log("[ERROR] Cannot queue buffer %d\n", i);
        }
    }

    // Start streaming
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (ioctl(g_video_fd, VIDIOC_STREAMON, &type) < 0) {
        kvm_log("[ERROR] Cannot start streaming: %s\n", strerror(errno));
        close(g_video_fd);
        g_video_fd = -1;
        return NULL;
    }

    kvm_log("[INFO] Streaming started\n");

    // Wait for encoder to produce first IDR frame
    kvm_log("[INFO] Waiting for encoder to initialize...\n");
    sleep(2);
    kvm_log("[INFO] Starting capture loop\n");

    // Capture loop
    fd_set fds;
    struct timeval tv;
    int frame_count = 0;
    // Removed max_frames limit to allow continuous streaming
    // int max_frames = 10000;

    while (g_running && g_capture_started) {
        FD_ZERO(&fds);
        FD_SET(g_video_fd, &fds);
        tv.tv_sec = 2;
        tv.tv_usec = 0;

        int r = select(g_video_fd + 1, &fds, NULL, NULL, &tv);
        if (r < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (r == 0) {
            continue;
        }

        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.length = 1;
        memset(&planes[0], 0, sizeof(planes[0]));
        buf.m.planes = &planes[0];

        if (ioctl(g_video_fd, VIDIOC_DQBUF, &buf) < 0) {
            kvm_log("[ERROR] DQBUF failed\n");
            break;
        }

        // Send to encoder
        if (g_encoder && g_buffers[buf.index].start[0] && g_ion_inited) {
            // Copy Y plane
            memcpy(gIonMem.vir, g_buffers[buf.index].start[0], WIDTH * HEIGHT);
            // Copy UV plane
            memcpy((unsigned char*)gIonMem.vir + WIDTH * HEIGHT,
                   (unsigned char*)g_buffers[buf.index].start[0] + WIDTH * HEIGHT,
                   WIDTH * HEIGHT / 2);

            // Flush cache
            flushCache(MEM_TYPE_CDX_NEW, &gIonMem, NULL);

            AVPacket packet;
            memset(&packet, 0, sizeof(AVPacket));

            packet.pAddrPhy0 = (unsigned char*)gIonMem.phy;
            packet.dataLen0 = WIDTH * HEIGHT;

            packet.pAddrPhy1 = (unsigned char*)gIonMem.phy + WIDTH * HEIGHT;
            packet.dataLen1 = WIDTH * HEIGHT / 2;

            packet.pts = frame_count;
            packet.id = frame_count;

            int ret = g_encoder->encode(&packet);
            if (ret < 0) {
                kvm_log("[WARN] Encode failed: %d\n", ret);
            }
        }

        // Re-queue buffer
        if (ioctl(g_video_fd, VIDIOC_QBUF, &buf) < 0) {
            kvm_log("[ERROR] QBUF failed\n");
        }

        frame_count++;
    }

    kvm_log("[INFO] Capture thread stopped, captured %d frames\n", frame_count);

    // --- MUST CLEANUP HARDWARE RESOURCES HERE ---
    // Stop streaming
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (ioctl(g_video_fd, VIDIOC_STREAMOFF, &type) < 0) {
        kvm_log("[WARN] VIDIOC_STREAMOFF failed: %s\n", strerror(errno));
    }

    // Unmap buffers
    for (int i = 0; i < g_buffer_count; i++) {
        if (g_buffers[i].start[0]) {
            if (munmap(g_buffers[i].start[0], g_buffers[i].length[0]) < 0) {
                kvm_log("[WARN] munmap failed for buffer %d\n", i);
            }
            g_buffers[i].start[0] = NULL;
        }
    }

    // Close device
    if (g_video_fd >= 0) {
        kvm_log("[INFO] Closing video device fd=%d\n", g_video_fd);
        close(g_video_fd);
        g_video_fd = -1;
    }
    // ---------------------------------------------

    return NULL;
}

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    // Disable stdout buffering
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    kvm_log("===========================================\n");
    kvm_log("  T527 Video for luckfox_kvm v%s\n", VIDEO_VERSION);
    kvm_log("  Resolution: %dx%d @ %dfps\n", WIDTH, HEIGHT, FPS);
    kvm_log("  Bitrate: %dkbps\n", BITRATE);
    kvm_log("  Ctrl Socket: %s\n", CTRL_SOCKET);
    kvm_log("  Video Socket: %s\n", VIDEO_SOCKET_PATH);
    kvm_log("===========================================\n");

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    kvm_log("[MAIN] Connecting to luckfox_kvm...\n");

    // CRITICAL: Must connect to luckfox_kvm BEFORE initializing encoder
    // because encoder init produces frames immediately
    int connect_retry = 0;
    while (connect_retry < 120) {
        // Connect to ctrl socket
        g_ctrl_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (g_ctrl_fd >= 0) {
            struct sockaddr_un addr;
            memset(&addr, 0, sizeof(addr));
            addr.sun_family = AF_UNIX;
            strncpy(addr.sun_path, CTRL_SOCKET, sizeof(addr.sun_path) - 1);
            if (connect(g_ctrl_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                close(g_ctrl_fd);
                g_ctrl_fd = -1;
            }
        }

        // Connect to video Unix Socket - MUST succeed before encoder init
        if (g_video_socket_fd < 0) {
            g_video_socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
            if (g_video_socket_fd >= 0) {
                struct sockaddr_un addr;
                memset(&addr, 0, sizeof(addr));
                addr.sun_family = AF_UNIX;
                strncpy(addr.sun_path, VIDEO_SOCKET_PATH, sizeof(addr.sun_path) - 1);
                if (connect(g_video_socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                    close(g_video_socket_fd);
                    g_video_socket_fd = -1;
                }
            }
            if (g_video_socket_fd >= 0) {
                kvm_log("[INFO] Connected video socket: %s (fd=%d)\n", VIDEO_SOCKET_PATH, g_video_socket_fd);
            } else {
                kvm_log("[WARN] Failed to connect video socket: %s (errno=%d)\n", VIDEO_SOCKET_PATH, errno);
            }
        }

        if (g_ctrl_fd >= 0 && g_video_socket_fd >= 0) {
            kvm_log("[INFO] CONNECTED: ctrl_fd=%d, video_socket_fd=%d\n", g_ctrl_fd, g_video_socket_fd);
            break;
        }

        // Close and retry
        if (g_ctrl_fd >= 0) { close(g_ctrl_fd); g_ctrl_fd = -1; }
        if (g_video_socket_fd >= 0) { close(g_video_socket_fd); g_video_socket_fd = -1; }

        kvm_log("[MAIN] Waiting for luckfox_kvm... retry %d/120\n", connect_retry + 1);
        sleep(2);
        connect_retry++;
    }

    if (g_ctrl_fd < 0 || g_video_socket_fd < 0) {
        kvm_log("[ERROR] Failed to connect to luckfox_kvm after 120 retries\n");
        kvm_log("[ERROR] ctrl_fd=%d, video_socket_fd=%d\n", g_ctrl_fd, g_video_socket_fd);
        // Continue anyway - encoder will produce frames, we just can't send them
    }

    // Init Ion memory
    if (init_ion_memory() < 0) {
        return -1;
    }

    // Init encoder - only after socket is ready
    kvm_log("[MAIN] Initializing encoder...\n");
    if (init_encoder() < 0) {
        free_ion_memory();
        return -1;
    }

    kvm_log("[MAIN] Ready, waiting for control commands...\n");

    // Wait for ctrl socket (should already be connected)
    int wait_count = 0;
    while (g_ctrl_fd < 0 && wait_count < 30) {
        kvm_log("[MAIN] Waiting for ctrl socket... %d/30\n", wait_count + 1);
        sleep(1);
        wait_count++;
    }

    if (g_ctrl_fd < 0) {
        kvm_log("[MAIN] Ctrl socket not ready, socket not connected...\n");
    } else {
        kvm_log("[MAIN] Ctrl socket ready, waiting for start_video command...\n");
    }

    // Start running state
    g_running = true;

    // Start control command processing thread
    pthread_t ctrl_thread;
    pthread_create(&ctrl_thread, NULL, control_thread, NULL);

    // Wait for control thread
    pthread_join(ctrl_thread, NULL);

    // Stop capture if still running
    pthread_mutex_lock(&g_cap_mutex);
    if (g_capture_started && g_cap_thread) {
        g_capture_started = false;
        pthread_mutex_unlock(&g_cap_mutex);
        pthread_join(g_cap_thread, NULL);
    } else {
        pthread_mutex_unlock(&g_cap_mutex);
    }

    // Cleanup
    if (g_encoder) {
        AWVideoEncoder::destroy(g_encoder);
        g_encoder = NULL;
    }

    if (g_callback) {
        delete g_callback;
        g_callback = NULL;
    }

    // Free Ion memory
    free_ion_memory();

    // Free buffers
    if (g_buffers) {
        for (int i = 0; i < g_buffer_count; i++) {
            if (g_buffers[i].start[0]) {
                munmap(g_buffers[i].start[0], g_buffers[i].length[0]);
            }
        }
        free(g_buffers);
    }

    // Close sockets
    if (g_ctrl_fd >= 0) close(g_ctrl_fd);
    if (g_video_socket_fd >= 0) close(g_video_socket_fd);

    kvm_log("[INFO] Demo finished\n");
    return 0;
}
