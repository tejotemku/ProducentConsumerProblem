#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <unistd.h>

#define QSources 100  // quantity of sources
#define QChannels 3   // quantity of channels
#define QBuffers 4     // quantity of buffers
#define SBuffer 501   // size of buffer
#define SPackage 64   // size of package
#define VPacket 1     // starting value of packet
#define MaxPacket 100 // max size of packet
#define VStream 201   // starting value of stream
#define MinStream 30  // min size of stream
#define MaxStream 500 // max size of stream
#define Shared 0

static volatile int RUN = 1;
unsigned long long int packet_produced = 0, packet_recived = 0;
unsigned long long int stream_produced = 0, stream_recived = 0;
sem_t any_empty;
sem_t any_full;

// buffer
typedef struct buffer_node {
    int data;
    struct buffer_node* next;
} buffer_node_t;

typedef struct buffer {
    buffer_node_t* head;
    sem_t empty;
} buffer_t;

buffer_t buffers[QBuffers];
pthread_t sources[QSources];
pthread_t channels[QChannels];


void buffer_clear(buffer_t* buff){
    buffer_node_t* node = buff->head;
    while(node != NULL)
    {
        node->data = 0;
        node = node->next;
    } 
}

void buffer_init(buffer_t* buff, int id) {
    buff->head = calloc(1, sizeof(buffer_node_t));
    buffer_node_t* node = buff->head;
    node->data = 0;
    for (int i = 0; i < SBuffer; i++)
    {
        node->next = calloc(1, sizeof(buffer_node_t));
        node = node->next;
        node->data = 0;
    }
    node->next = NULL;
    sem_init(&buff->empty, Shared, 1);
    buffer_clear(buff);
}


// producer
void produce_stream (buffer_t* buff) {
    int start = VStream;
    int end = start + rand() % (MaxStream - MinStream) + MinStream;
    stream_produced += (end-start);
    buffer_node_t* node = buff -> head;
    for (int i = start; i < end; i++)
    {
        node->data  = i;
        node = node->next;
    }
}

void produce_packet (buffer_t* buff) {
    int size = rand() % MaxPacket;
    packet_produced += size;
    buffer_node_t* node = buff -> head;
    for (int i = 0; i < size; i++) {
        node->data  = rand() % MaxPacket + 1;
        node = node->next;
    }
}

void* source() {
    int is_packet_producer = rand() % 2; // determines if source produces packets or streams

    while(RUN)
    {
        sem_wait(&any_empty);
        for (int i = 0; i<QBuffers; i++)
        {
            if(sem_trywait(&buffers[i].empty) == 0)
            {
                if (is_packet_producer == 1) 
                {
                   produce_packet(&buffers[i]);
                } 
                else
                {
                   produce_stream(&buffers[i]);
                }
                sem_post(&any_full);
                break;
            }
        }
    
    }
    return NULL;
}

// consumer
void* channel() {
    int iterator = 0;
    int package[SPackage]; 
    while(RUN)
    {
        sem_wait(&any_full);
        for (int i = 0; i<QBuffers; i++)
        {
            if(sem_trywait(&buffers[i].empty) != 0)
            {
                buffer_node_t* node = buffers[i].head;
                int size = 0;
                int is_packet = node-> data < 101 ? 1 : 0;
                while(1)
                {
                    int d = node->data;
                    if (d == 0) break;
                    size++;
                    package[iterator] = d;
                    node = node->next;
                    iterator = (iterator + 1) % SPackage;
                }
                size--;
                if (is_packet == 1) packet_recived += size;
                else stream_recived += size;

                buffer_clear(&buffers[i]);
                sem_post(&buffers[i].empty);
                sem_post(&any_empty);
                break;
            }
        }
    }
    return NULL;
}



// overseer
void* overrseer() {
    int round = 0;
    while(RUN)
    {
        usleep(2000*1000); // 2 seconds
        printf("Round: %d\n", round);
        if (packet_produced != 0) printf("packet recived ratio: %lld/%lld - %lld%%\n", packet_recived, packet_produced, (packet_recived*100)/packet_produced);
        else printf("no packet produced\n");
        if (stream_produced != 0) printf("stream recived ratio: %lld/%lld - %lld%%\n", stream_recived, stream_produced, (stream_recived*100)/stream_produced);
        else printf("no stream produced\n");
        round ++;
    }
    return NULL;
}


// main
int main() {
    sem_init(&any_empty, Shared, 0);
    sem_init(&any_full, Shared, SBuffer);
    for (int i=0; i<QBuffers; i++)
        buffer_init(&buffers[i], i);
    for (int i = 0; i<QSources; i++)
        pthread_create(&sources[i], NULL, source, NULL);
    for (int i = 0; i<QChannels; i++)
        pthread_create(&channels[i], NULL, channel, NULL); 
    pthread_t overrseer_id;
    pthread_create(&overrseer_id, NULL, overrseer, NULL);
    getchar();
    RUN = 0;
    return 0;
}