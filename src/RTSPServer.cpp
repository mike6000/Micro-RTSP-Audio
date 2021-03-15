#include "RTSPServer.h"
#include "CRtspSession.h"

RTSPServer::RTSPServer(AudioStreamer * streamer, int port) {
    this->streamer = streamer;
    this->port = port;
}

int RTSPServer::runAsync() {
    int error;

    printf("running RTSP server\n");

    ServerAddr.sin_family      = AF_INET;
    ServerAddr.sin_addr.s_addr = INADDR_ANY;
    ServerAddr.sin_port        = htons(port);                 // listen on RTSP port 8554 as default
    int s = socket(AF_INET,SOCK_STREAM,0);
    printf("Master socket fd: %i\n", s);
    MasterSocket               = new WiFiClient(s);
    if (MasterSocket == NULL) {
        printf("MasterSocket object couldnt be created\n");
        return -1;
    }

    printf("Master Socket created; fd: %i\n", MasterSocket->fd());

    int enable = 1;
    error = setsockopt(MasterSocket->fd(), SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    if (error < 0) {
        printf("setsockopt(SO_REUSEADDR) failed");
        return error;
    }

    printf("Socket options set\n");

    // bind our master socket to the RTSP port and listen for a client connection
    error = bind(MasterSocket->fd(),(sockaddr*)&ServerAddr,sizeof(ServerAddr));
    if (error != 0) {
        printf("error can't bind port errno=%d\n", errno);
        return error;
    }
    printf("Socket bound. Starting to listen\n");
    error = listen(MasterSocket->fd(),5);
    if (error != 0) {
        printf("Error while listening\n");
        return error;
    }

    if (xTaskCreatePinnedToCore(RTSPServer::serverThread, "RTSPServerThread", 10000, (void*)this, 5, &workerHandle, 1) != pdPASS) {
        printf("Couldn't create server thread");
        return -1;
    } 


    return 0;
}

void RTSPServer::serverThread(void* server_obj) {
    socklen_t ClientAddrLen = sizeof(ClientAddr);
    RTSPServer * server = (RTSPServer*) server_obj;
   
    printf("Server thread listening...\n");

    while (true)
    {   // loop forever to accept client connections
        // only allow one client at a time
        
        if (server->numClients == 0) {
            server->ClientSocket = new WiFiClient(accept(server->MasterSocket->fd(),(struct sockaddr*)&server->ClientAddr,&ClientAddrLen));
            printf("Client connected. Client address: %s\n",inet_ntoa(server->ClientAddr.sin_addr));
            if (xTaskCreatePinnedToCore(RTSPServer::workerThread, "workerThread", 8000, (void*)server, 8, NULL, 1) != pdPASS) {
                printf("Couldn't create workerThread\n");
            } else {
                printf("Created workerThread\n");
                server->numClients++;
            }
        } else {
            vTaskDelay(50);
        }

        vTaskDelay(10);
        
        //vTaskResume(workerHandle);
        // TODO only ONE task used repeatedly
    }


    // should never be reached
    closesocket(server->MasterSocket);

    printf("Error: %s is returning\n", pcTaskGetTaskName(NULL));
}


void RTSPServer::workerThread(void * server_obj) {
    RTSPServer * server = (RTSPServer*)server_obj;
    AudioStreamer * streamer = server->streamer;
    SOCKET s = server->ClientSocket;

        // stop this task - wait for a client to connect
        //vTaskSuspend(NULL);
        // TODO check if everything is ok to go
        printf("Client connected\n");

        CRtspSession * rtsp = new CRtspSession(*s, streamer);     // our threads RTSP session and state

        printf("Session ready\n");

        while (rtsp->m_sessionOpen)
        {
            uint32_t timeout = 400;
            //printf("Handling incoming requests\n");
            if(!rtsp->handleRequests(timeout)) {
                //printf("Request handling returned false\n");
                struct timeval now;
                gettimeofday(&now, NULL); // crufty msecish timer
                //uint32_t msec = now.tv_sec * 1000 + now.tv_usec / 1000;
                //rtsp.broadcastCurrentFrame(msec);
                //printf("Audio File has been sent\n");
            } else {
                //printf("Request handling successful\n");
            }

            if (rtsp->m_streaming) {
                // Stream RTP data
                //streamer->Start();
            }

            vTaskDelay(50/portTICK_PERIOD_MS);
        }

    
    // should never be reached
    printf("workerThread stopped, deleting task\n");
    delete rtsp;
    server->numClients--;

    vTaskDelete(NULL);
}