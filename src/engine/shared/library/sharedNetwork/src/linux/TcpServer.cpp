// TcpServer.cpp
// Copyright 2000-02, Sony Online Entertainment Inc., all rights reserved. 
// Author: Justin Randall

//---------------------------------------------------------------------

#include "sharedNetwork/FirstSharedNetwork.h"
#include "TcpServer.h"

#include "sharedNetwork/Connection.h"
#include "sharedNetwork/Service.h"
#include "TcpClient.h"
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <vector>

//---------------------------------------------------------------------

TcpServer::TcpServer(Service * service, const std::string & a, const unsigned short port) :
m_bindAddress(a, port),
m_handle(-1),
m_service(service),
m_connections(),
m_connectionSockets(),
m_inputBuffer(0),
m_inputBufferSize(0)
{
	protoent * p = getprotobyname("tcp");
	if(p)
	{
		int entry = p->p_proto;
		m_handle = socket(AF_INET, SOCK_STREAM, entry);
		FATAL(m_handle == -1, ("Failed to create a server socket %s:%d", a.c_str(), port));
		int optval = 1;
		if(m_handle != -1)
		{
			int optResult = setsockopt(m_handle, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
			FATAL(optResult == -1, ("Failed to set socket option SO_REUSEADDR"));
			unsigned long nb = 1;
			int ctlResult = ioctl(m_handle, FIONBIO, &nb);
			FATAL(ctlResult == -1, ("Failed to set socket non-blocking"));
			socklen_t addrlen = sizeof(struct sockaddr_in);
			int bindResult = bind(m_handle, (struct sockaddr *)(&m_bindAddress.getSockAddr4()), addrlen);
			FATAL(bindResult != 0, ("Failed to bind to port %d", port));
			if(bindResult == 0)
			{
				int result = listen(m_handle, 256);
				if(result == -1)
					perror("listen ");
				FATAL(result == -1, ("Failed to start listening on port %d", port));
				struct sockaddr_in b;
				getsockname(m_handle, (struct sockaddr *)(&b), &addrlen);
				m_bindAddress = b;
			}
		}
	}
}

//---------------------------------------------------------------------

TcpServer::~TcpServer()
{
    // Close the server socket
    if (m_handle != -1)
    {
        close(m_handle);
    }

    // Notify all clients of the server's closure
    for (auto &entry : m_connections)
    {
        TcpClient *client = entry.second;
        if (client)
        {
            client->clearTcpServer();
        }
    }

    // Optionally, clean up the clients if needed
    // Make sure you don't delete clients that are still in use elsewhere
    for (auto &client : m_pendingDestroys)
    {
        delete client;
    }
}

//---------------------------------------------------------------------

const std::string & TcpServer::getBindAddress() const
{
    return m_bindAddress.getHostAddress();
}



//---------------------------------------------------------------------

const unsigned short TcpServer::getBindPort() const
{
    return m_bindAddress.getHostPort();
}


//---------------------------------------------------------------------

void TcpServer::onConnectionClosed(TcpClient *c)
{
    if (c)
    {
        // Ensure that the client is not already in the set
        auto result = m_pendingDestroys.insert(c);
        // Optionally handle the case where the insertion failed (which should not happen)
        // For example, check result.second to see if the insertion was successful
    }
}


//---------------------------------------------------------------------
void TcpServer::removeClient(TcpClient * c)
{
	std::map<int, TcpClient *>::iterator f = m_connections.find(c->getSocket());
	if(f != m_connections.end())
	{
//		c->release();
		m_connections.erase(f);
	}
	
	std::vector<pollfd>::iterator s;
	for(s = m_connectionSockets.begin(); s != m_connectionSockets.end(); ++s)
	{
		if((*s).fd == c->getSocket())
		{
			m_connectionSockets.erase(s);
			break;
		}
	}
}

//---------------------------------------------------------------------

void TcpServer::update()
{
    // Handle new incoming connections
    struct pollfd listenPfd = { m_handle, POLLIN, 0 };
    int result = poll(&listenPfd, 1, 0);

    if (result > 0 && (listenPfd.revents & POLLIN))
    {
        // Connection established
        struct sockaddr_in addr;
        socklen_t addrLen = sizeof(addr);
        int newSock = accept(m_handle, (struct sockaddr *)(&addr), &addrLen);

        if (newSock != -1)
        {
            // Create a new TcpClient for the accepted connection
            TcpClient *ptc = new TcpClient(newSock, this);
            ptc->addRef();
            m_connections[newSock] = ptc;

            // Add new client socket to the poll list
            struct pollfd readFd = { newSock, POLLIN | POLLERR | POLLHUP, 0 };
            m_connectionSockets.push_back(readFd);

            ptc->onConnectionOpened();
            if (m_service)
                m_service->onConnectionOpened(ptc);

            ptc->release();
        }
    }

    // Handle existing connections
    std::vector<struct pollfd> pollFds = m_connectionSockets;
    int readResult = poll(pollFds.data(), pollFds.size(), 0);

    if (readResult > 0)
    {
        for (const auto& pfd : pollFds)
        {
            if (pfd.revents & POLLERR)
            {
                auto it = m_connections.find(pfd.fd);
                if (it != m_connections.end())
                {
                    TcpClient *c = it->second;
                    if (c->m_connection)
                        c->m_connection->setDisconnectReason("TcpServer::update POLLERR");
                    c->onConnectionClosed();
                }
            }
            else if (pfd.revents & (POLLIN | POLLHUP))
            {
                if (m_inputBuffer == nullptr)
                {
                    m_inputBuffer = new unsigned char[1500];
                    m_inputBufferSize = 1500;
                }

                int bytesReceived = recv(pfd.fd, m_inputBuffer, m_inputBufferSize, 0);
                auto it = m_connections.find(pfd.fd);

                if (it != m_connections.end())
                {
                    TcpClient *c = it->second;
                    if (m_pendingDestroys.find(c) == m_pendingDestroys.end())
                    {
                        c->addRef();
                        if (bytesReceived > 0)
                        {
                            c->onReceive(m_inputBuffer, bytesReceived);
                        }
                        else if (bytesReceived == 0)
                        {
                            if (c->m_connection)
                                c->m_connection->setDisconnectReason("TcpServer::update recv returned 0 bytes");
                            c->onConnectionClosed();
                        }
                        else if (bytesReceived == -1)
                        {
                            if (c->m_connection)
                                c->m_connection->setDisconnectReason("TcpServer::update recv error");
                            c->onConnectionClosed();
                        }
                        c->release();
                    }
                }

                // Handle buffer resizing if needed
                if (bytesReceived == m_inputBufferSize)
                {
                    delete[] m_inputBuffer;
                    m_inputBufferSize *= 2;
                    m_inputBuffer = new unsigned char[m_inputBufferSize];
                }
            }
        }

        // Cleanup pending destroys
        for (TcpClient *client : m_pendingDestroys)
        {
            removeClient(client);
        }
        m_pendingDestroys.clear();
    }
}


//---------------------------------------------------------------------

