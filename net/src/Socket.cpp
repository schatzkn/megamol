/*
 * Socket.cpp
 *
 * Copyright (C) 2006 by Universitaet Stuttgart (VIS). Alle Rechte vorbehalten.
 * Copyright (C) 2005 by Christoph Mueller (christoph.mueller@vis.uni-stuttgart.de). All rights reserved.
 */

#include <cstdlib>

#ifndef _WIN32
#include <unistd.h>

#define SOCKET_ERROR (-1)
#endif /* _WIN32 */

#include "vislib/Socket.h"

#include "vislib/assert.h"
#include "vislib/error.h"
#include "vislib/IllegalParamException.h"
#include "vislib/SocketException.h"
#include "vislib/Trace.h"
#include "vislib/UnsupportedOperationException.h"


/*
 * vislib::net::Socket::Cleanup
 */
void vislib::net::Socket::Cleanup(void) {
#ifdef _WIN32
    if (::WSACleanup() != 0) {
        throw SocketException(__FILE__, __LINE__);
    }
#endif /* _WIN32 */
}


/*
 * vislib::net::Socket::Startup
 */
void vislib::net::Socket::Startup(void) {
#ifdef _WIN32
    WSAData wsaData;

    // Note: Need Winsock 2 for timeouts.
    if (::WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        throw SocketException(__FILE__, __LINE__);
    }
#endif /* _WIN32 */
}


/*
 * vislib::net::Socket::TIMEOUT_INFINITE 
 */
const UINT vislib::net::Socket::TIMEOUT_INFINITE = 0;


/*
 * vislib::net::Socket::~Socket
 */
vislib::net::Socket::~Socket(void) {
}


/*
 * vislib::net::Socket::Accept
 */
vislib::net::Socket vislib::net::Socket::Accept(SocketAddress *outConnAddr) {
    struct sockaddr connAddr;
    SOCKET newSocket;

#ifdef _WIN32
    INT addrLen = static_cast<int>(sizeof(connAddr));

    if ((newSocket = ::WSAAccept(this->handle, &connAddr, &addrLen, NULL, 
            0)) == INVALID_SOCKET) {
        throw SocketException(__FILE__, __LINE__);
    }

#else /* _WIN32 */
    unsigned int addrLen = static_cast<unsigned int>(sizeof(connAddr));

    if ((newSocket = ::accept(this->handle, &connAddr, &addrLen))
            == SOCKET_ERROR) {
        throw SocketException(__FILE__, __LINE__);
    }

#endif /* _WIN32 */

    if (outConnAddr != NULL) {
        *outConnAddr = SocketAddress(connAddr);
    }

    return Socket(newSocket);
}


/*
 * vislib::net::Socket::Bind
 */
void vislib::net::Socket::Bind(const SocketAddress& address) {
    if (::bind(this->handle, static_cast<const struct sockaddr *>(address),
            sizeof(struct sockaddr_in)) == SOCKET_ERROR) {
        throw SocketException(__FILE__, __LINE__);
    }
}


/*
 * vislib::net::Socket::Close
 */
void vislib::net::Socket::Close(void) {

    if (this->IsValid()) {

#ifdef _WIN32
        if (::closesocket(this->handle) == SOCKET_ERROR) {
#else /* _WIN32 */
        if (::close(this->handle) == SOCKET_ERROR) {
#endif /* _WIN32 */
            throw SocketException(__FILE__, __LINE__);
        }

        this->handle = INVALID_SOCKET;

    } /* end if (this->IsValid()) */
}


/*
 * vislib::net::Socket::Connect
 */
void vislib::net::Socket::Connect(const SocketAddress& address) {
#ifdef _WIN32
    if (::WSAConnect(this->handle, static_cast<const struct sockaddr *>(
            address), sizeof(struct sockaddr), NULL, NULL, NULL, NULL) 
            == SOCKET_ERROR) {
#else /* _WIN32 */
    if (::connect(this->handle, static_cast<const struct sockaddr *>(address),
            sizeof(struct sockaddr)) == SOCKET_ERROR) {
#endif /* _Win32 */
        throw SocketException(__FILE__, __LINE__);
    }
}


/*
 * vislib::net::Socket::Create
 */
void vislib::net::Socket::Create(const ProtocolFamily protocolFamily, 
                                 const Type type, 
                                 const Protocol protocol) {
#ifdef _WIN32
    this->handle = ::WSASocket(static_cast<const int>(protocolFamily), 
        static_cast<const int>(type), static_cast<const int>(protocol),
        NULL, 0, WSA_FLAG_OVERLAPPED);
#else /* _WIN32 */
    this->handle = ::socket(static_cast<const int>(protocolFamily), 
        static_cast<const int>(type), static_cast<const int>(protocol));
#endif /* _WIN32 */

    if (this->handle == INVALID_SOCKET) {
        throw SocketException(__FILE__, __LINE__);
    }
}


/*
 * vislib::net::Socket::GetOption
 */
void vislib::net::Socket::GetOption(const INT level, const INT optName, 
        void *outValue, SIZE_T& inOutValueLength) const {
#ifdef _WIN32
	int len = static_cast<INT>(inOutValueLength);
#else /* _WIN32 */
	socklen_t len = static_cast<socklen_t>(inOutValueLength);
#endif /* _WIN32 */

    if (::getsockopt(this->handle, level, optName, 
            static_cast<char *>(outValue), &len) != 0) {
        throw SocketException(__FILE__, __LINE__);
    }

	inOutValueLength = static_cast<SIZE_T>(len);
}


/*
 * vislib::net::Socket::IOControl
 */
void vislib::net::Socket::IOControl(const DWORD ioControlCode, void *inBuffer,
        const DWORD cntInBuffer, void *outBuffer, const DWORD cntOutBuffer,
        DWORD& outBytesReturned) {
#ifdef _WIN32
    if (::WSAIoctl(this->handle, ioControlCode, inBuffer, cntInBuffer, 
            outBuffer, cntOutBuffer, &outBytesReturned, NULL, NULL) 
            == SOCKET_ERROR) {
        throw SocketException(__FILE__, __LINE__);
    }
#else /* _WIN32 */
    // TODO
#endif /* _WIN32 */
}

/*
 * vislib::net::Socket::Listen
 */
void vislib::net::Socket::Listen(const INT backlog) {
    if (::listen(this->handle, backlog) == SOCKET_ERROR) {
        throw SocketException(__FILE__, __LINE__);
    }
}


/*
 * vislib::net::Socket::Receive
 */
SIZE_T vislib::net::Socket::Receive(void *outData, const SIZE_T cntBytes, 
        const INT timeout, const INT flags, const bool forceReceive) {
    int n = 0;                  // Highest descriptor in 'readSet' + 1.
    fd_set readSet;             // Set of socket to check for readability.
    struct timeval timeOut;     // Timeout for readability check.

    /* Check parameter constraints. */
    if ((timeout >= 1) && forceReceive) {
        throw IllegalParamException("forceReceive", __FILE__, __LINE__);
    }

    /* Handle infinite timeout first by calling normal receive operation. */
    if (timeout < 1) {
        return this->receive(outData, cntBytes, flags, forceReceive);
    }

    /* Initialise socket set and timeout structure. */
    ASSERT(forceReceive == false);
    FD_ZERO(&readSet);
    FD_SET(this->handle, &readSet);

    timeOut.tv_sec = timeout / 1000;
    timeOut.tv_usec = (timeout % 1000) * 1000;

    /* Wait for the socket to become readable. */
#ifndef _WIN32
    n = this->handle + 1;   // Windows does not need 'n' and will ignore it.
#endif /* !_WIN32 */
    if (::select(n, &readSet, NULL, NULL, &timeOut) == -1) {
        throw SocketException(__FILE__, __LINE__);
    }

    if (FD_ISSET(this->handle, &readSet)) {
        /* Delegate to normal receive operation. */
        return this->receive(outData, cntBytes, flags, forceReceive);

    } else {
        /* Signal timeout. */
#ifdef _WIN32
        throw SocketException(WSAETIMEDOUT, __FILE__, __LINE__);
#else /* _WIN32 */
        throw SocketException(ETIME, __FILE__, __LINE__);
#endif /* _WIN32 */
    } 
}


/*
 * vislib::net::Socket::Receive
 */
SIZE_T vislib::net::Socket::Receive(SocketAddress& outFromAddr, void *outData, 
        const SIZE_T cntBytes, const INT timeout, const INT flags,  
        const bool forceReceive) {
    int n = 0;                  // Highest descriptor in 'readSet' + 1.
    fd_set readSet;             // Set of socket to check for readability.
    struct timeval timeOut;     // Timeout for readability check.

    /* Check parameter constraints. */
    if ((timeout >= 1) && forceReceive) {
        throw IllegalParamException("forceReceive", __FILE__, __LINE__);
    }

    /* Handle infinite timeout first by calling normal receive operation. */
    if (timeout < 1) {
        return this->receiveFrom( outFromAddr, outData, cntBytes,flags, 
            forceReceive);
    }

    /* Initialise socket set and timeout structure. */
    ASSERT(forceReceive == false);
    FD_ZERO(&readSet);
    FD_SET(this->handle, &readSet);

    timeOut.tv_sec = timeout / 1000;
    timeOut.tv_usec = (timeout % 1000) * 1000;

    /* Wait for the socket to become readable. */
#ifndef _WIN32
    n = this->handle + 1;   // Windows does not need 'n' and will ignore it.
#endif /* !_WIN32 */
    if (::select(n, &readSet, NULL, NULL, &timeOut) == -1) {
        throw SocketException(__FILE__, __LINE__);
    }

    if (FD_ISSET(this->handle, &readSet)) {
        /* Delegate to normal receive operation. */
        return this->receiveFrom(outFromAddr, outData, cntBytes, flags, 
            forceReceive);

    } else {
        /* Signal timeout. */
#ifdef _WIN32
        throw SocketException(WSAETIMEDOUT, __FILE__, __LINE__);
#else /* _WIN32 */
        throw SocketException(ETIME, __FILE__, __LINE__);
#endif /* _WIN32 */
    } 
}


/*
 * vislib::net::Socket::Send
 */
SIZE_T vislib::net::Socket::Send(const void *data, const SIZE_T cntBytes, 
        const INT flags, const INT timeout, const bool forceSend) {
    int n = 0;                  // Highest descriptor in 'writeSet' + 1.
    fd_set writeSet;            // Set of socket to check for writability.
    struct timeval timeOut;     // Timeout for writability check.

    /* Check parameter constraints. */
    if ((timeout >= 1) && forceSend) {
        throw IllegalParamException("forceSend", __FILE__, __LINE__);
    }

    /* Handle infinite timeout first by calling normal send operation. */
    if (timeout < 1) {
        return this->send(data, cntBytes, flags, forceSend);
    }

    /* Initialise socket set and timeout structure. */
    ASSERT(forceSend == false);
    FD_ZERO(&writeSet);
    FD_SET(this->handle, &writeSet);

    timeOut.tv_sec = timeout / 1000;
    timeOut.tv_usec = (timeout % 1000) * 1000;

    /* Wait for the socket to become readable. */
#ifndef _WIN32
    n = this->handle + 1;   // Windows does not need 'n' and will ignore it.
#endif /* !_WIN32 */
    if (::select(n, &writeSet, NULL, NULL, &timeOut) == -1) {
        throw SocketException(__FILE__, __LINE__);
    }

    if (FD_ISSET(this->handle, &writeSet)) {
        /* Delegate to normal send operation. */
        return this->send(data, cntBytes, flags, forceSend);

    } else {
        /* Signal timeout. */
#ifdef _WIN32
        throw SocketException(WSAETIMEDOUT, __FILE__, __LINE__);
#else /* _WIN32 */
        throw SocketException(ETIME, __FILE__, __LINE__);
#endif /* _WIN32 */
    }
}


/*
 * vislib::net::Socket::Send
 */
SIZE_T vislib::net::Socket::Send(const SocketAddress& toAddr, const void *data,
        const SIZE_T cntBytes, const INT timeout, const INT flags,
        const bool forceSend) {
    int n = 0;                  // Highest descriptor in 'writeSet' + 1.
    fd_set writeSet;            // Set of socket to check for writability.
    struct timeval timeOut;     // Timeout for writability check.

    /* Check parameter constraints. */
    if ((timeout >= 1) && forceSend) {
        throw IllegalParamException("forceSend", __FILE__, __LINE__);
    }

    /* Handle infinite timeout first by calling normal send operation. */
    if (timeout < 1) {
        return this->sendTo(toAddr, data, cntBytes, flags, forceSend);
    }

    /* Initialise socket set and timeout structure. */
    ASSERT(forceSend == false);
    FD_ZERO(&writeSet);
    FD_SET(this->handle, &writeSet);

    timeOut.tv_sec = timeout / 1000;
    timeOut.tv_usec = (timeout % 1000) * 1000;

    /* Wait for the socket to become readable. */
#ifndef _WIN32
    n = this->handle + 1;   // Windows does not need 'n' and will ignore it.
#endif /* !_WIN32 */
    if (::select(n, &writeSet, NULL, NULL, &timeOut) == -1) {
        throw SocketException(__FILE__, __LINE__);
    }

    if (FD_ISSET(this->handle, &writeSet)) {
        /* Delegate to normal send operation. */
        return this->sendTo(toAddr, data, cntBytes, flags, forceSend);

    } else {
        /* Signal timeout. */
#ifdef _WIN32
        throw SocketException(WSAETIMEDOUT, __FILE__, __LINE__);
#else /* _WIN32 */
        throw SocketException(ETIME, __FILE__, __LINE__);
#endif /* _WIN32 */
    }
}


/*
 * vislib::net::Socket::SetOption
 */
void vislib::net::Socket::SetOption(const INT level, const INT optName, 
            const void *value, const SIZE_T valueLength) {
    if (::setsockopt(this->handle, level, optName, 
            static_cast<const char *>(value), static_cast<int>(valueLength)) 
            != 0) {
        throw SocketException(__FILE__, __LINE__);
    }
}


/*
 * vislib::net::Socket::SetRcvAll
 */
void vislib::net::Socket::SetRcvAll(const bool enable) {
#ifdef _WIN32
    DWORD inBuffer = enable ? 1 : 0;
    DWORD bytesReturned;
    // SIO_RCVALL
    this->IOControl(_WSAIOW(IOC_VENDOR, 1), &inBuffer, sizeof(inBuffer), NULL, 
        0, bytesReturned);
#endif /* _WIN32 */
}


/*
 * vislib::net::Socket::Shutdown
 */
void vislib::net::Socket::Shutdown(const ShutdownManifest how) {
    if (::shutdown(this->handle, how) == SOCKET_ERROR) {
        throw SocketException(__FILE__, __LINE__);
    }
}


/*
 * vislib::net::Socket::operator =
 */
vislib::net::Socket& vislib::net::Socket::operator =(const Socket& rhs) {
    if (this != &rhs) {
        this->handle = rhs.handle;
    }

    return *this;
}


/*
 * vislib::net::Socket::operator ==
 */
bool vislib::net::Socket::operator ==(const Socket& rhs) const {
    return (this->handle == rhs.handle);
}


/*
 * vislib::net::Socket::receive
 */
SIZE_T vislib::net::Socket::receive(void *outData, const SIZE_T cntBytes, 
        const INT flags, const bool forceReceive) {
    SIZE_T totalReceived = 0;   // # of bytes totally received.
    INT lastReceived = 0;       // # of bytes received during last recv() call.
#ifdef _WIN32
    WSAOVERLAPPED overlapped;   // Overlap structure for asynchronous recv().
    WSABUF wsaBuf;              // Buffer for WSA output.
    DWORD errorCode = 0;        // WSA error during last operation.
    DWORD inOutFlags = flags;   // Flags for WSA.

    if ((overlapped.hEvent = ::WSACreateEvent()) == WSA_INVALID_EVENT) {
        throw SocketException(__FILE__, __LINE__);
    }

    wsaBuf.buf = static_cast<char *>(outData);
    wsaBuf.len = static_cast<u_long>(cntBytes);
#endif /* _WIN32 */

    do {
#ifdef _WIN32
        if (::WSARecv(this->handle, &wsaBuf, 1, reinterpret_cast<DWORD *>(
                &lastReceived), &inOutFlags, &overlapped, NULL) != 0) {
            if ((errorCode = ::WSAGetLastError()) != WSA_IO_PENDING) {
                ::WSACloseEvent(overlapped.hEvent);
                throw SocketException(errorCode, __FILE__, __LINE__);
            }
            TRACE(DEBUG_TRACE_LEVEL, "Overlapped socket I/O pending.\n");
            if (!::WSAGetOverlappedResult(this->handle, &overlapped, 
                    reinterpret_cast<DWORD *>(&lastReceived), TRUE, 
                    &inOutFlags)) {
                ::WSACloseEvent(overlapped.hEvent);
                throw SocketException(__FILE__, __LINE__);
            }
        }
        totalReceived += lastReceived;
        wsaBuf.buf += lastReceived;
        wsaBuf.len -= lastReceived;

#else /* _WIN32 */
        lastReceived = ::recv(this->handle, static_cast<char *>(outData) 
            + totalReceived, static_cast<int>(cntBytes - totalReceived), flags);

        if ((lastReceived >= 0) && (lastReceived != SOCKET_ERROR)) {
            /* Successfully received new package. */
            totalReceived += static_cast<SIZE_T>(lastReceived);

        } else {
            /* Communication failed. */
            throw SocketException(__FILE__, __LINE__);
        }

#endif /* _WIN32 */
    } while (forceReceive && (totalReceived < cntBytes));

#ifdef _WIN32
    if (!::WSACloseEvent(overlapped.hEvent)) {
        throw SocketException(__FILE__, __LINE__);
    }
#endif /*_WIN32 */

    return totalReceived;
}


/*
 * vislib::net::Socket::receiveFrom
 */
SIZE_T vislib::net::Socket::receiveFrom(SocketAddress& outFromAddr, 
        void *outData, const SIZE_T cntBytes, const INT flags, 
        const bool forceReceive) {
    SIZE_T totalReceived = 0;   // # of bytes totally received.
    INT lastReceived = 0;       // # of bytes received during last recv() call.
    struct sockaddr from;
#ifdef _WIN32
    WSAOVERLAPPED overlapped;   // Overlap structure for asynchronous recv().
    WSABUF wsaBuf;              // Buffer for WSA output.
    DWORD errorCode = 0;        // WSA error during last operation.
    DWORD inOutFlags = flags;   // Flags for WSA.
    INT fromLen = sizeof(from);

    if ((overlapped.hEvent = ::WSACreateEvent()) == WSA_INVALID_EVENT) {
        throw SocketException(__FILE__, __LINE__);
    }

    wsaBuf.buf = static_cast<char *>(outData);
    wsaBuf.len = static_cast<u_long>(cntBytes);
#else /* _WIN32 */
    socklen_t fromLen = sizeof(from);
#endif /* _WIN32 */

    do {
#ifdef _WIN32
        if (::WSARecvFrom(this->handle, &wsaBuf, 1, reinterpret_cast<DWORD *>(
                &lastReceived), &inOutFlags, &from, &fromLen, &overlapped, NULL) 
                != 0) {
            if ((errorCode = ::WSAGetLastError()) != WSA_IO_PENDING) {
                ::WSACloseEvent(overlapped.hEvent);
                throw SocketException(errorCode, __FILE__, __LINE__);
            }
            TRACE(DEBUG_TRACE_LEVEL, "Overlapped socket I/O pending.\n");
            if (!::WSAGetOverlappedResult(this->handle, &overlapped, 
                    reinterpret_cast<DWORD *>(&lastReceived), TRUE, 
                    &inOutFlags)) {
                ::WSACloseEvent(overlapped.hEvent);
                throw SocketException(__FILE__, __LINE__);
            }
        }
        totalReceived += lastReceived;
        wsaBuf.buf += lastReceived;
        wsaBuf.len -= lastReceived;

#else /* _WIN32 */
        lastReceived = ::recvfrom(this->handle, static_cast<char *>(outData) 
            + totalReceived, static_cast<int>(cntBytes - totalReceived), flags,
            &from, &fromLen);

        if ((lastReceived >= 0) && (lastReceived != SOCKET_ERROR)) {
            /* Successfully received new package. */
            totalReceived += static_cast<SIZE_T>(lastReceived);

        } else {
            /* Communication failed. */
            throw SocketException(__FILE__, __LINE__);
        }
#endif /* _WIN32 */

    } while (forceReceive && (totalReceived < cntBytes));

    outFromAddr = from;

#ifdef _WIN32
    if (!::WSACloseEvent(overlapped.hEvent)) {
        throw SocketException(__FILE__, __LINE__);
    }
#endif /*_WIN32 */

    return totalReceived;
}


/*
 * vislib::net::Socket::send
 */
SIZE_T vislib::net::Socket::send(const void *data, const SIZE_T cntBytes, 
        const INT flags, const bool forceSend) {
    SIZE_T totalSent = 0;       // # of bytes totally sent.      
    INT lastSent = 0;           // # of bytes sent during last send() call.
#ifdef _WIN32
    WSAOVERLAPPED overlapped;   // Overlap structure for asynchronous recv().
    WSABUF wsaBuf;              // Buffer for WSA output.
    DWORD errorCode = 0;        // WSA error during last operation.
    DWORD inOutFlags = flags;   // Flags for WSA.

    if ((overlapped.hEvent = ::WSACreateEvent()) == WSA_INVALID_EVENT) {
        throw SocketException(__FILE__, __LINE__);
    }

    wsaBuf.buf = const_cast<char *>(static_cast<const char *>(data));
    wsaBuf.len = static_cast<u_long>(cntBytes);
#endif /* _WIN32 */

    do {
#ifdef _WIN32
        if (::WSASend(this->handle, &wsaBuf, 1, reinterpret_cast<DWORD *>(
                &lastSent), flags, &overlapped, NULL) != 0) {
            if ((errorCode = ::WSAGetLastError()) != WSA_IO_PENDING) {
                ::WSACloseEvent(overlapped.hEvent);
                throw SocketException(errorCode, __FILE__, __LINE__);
            }
            TRACE(DEBUG_TRACE_LEVEL, "Overlapped socket I/O pending.\n");
            if (!::WSAGetOverlappedResult(this->handle, &overlapped, 
                    reinterpret_cast<DWORD *>(&lastSent), TRUE, &inOutFlags)) {
                ::WSACloseEvent(overlapped.hEvent);
                throw SocketException(__FILE__, __LINE__);
            }
        }
        totalSent += lastSent;
        wsaBuf.buf += lastSent;
        wsaBuf.len -= lastSent;

#else /* _WIN32 */
        lastSent = ::send(this->handle, static_cast<const char *>(data), 
            static_cast<int>(cntBytes - totalSent), flags);

        if ((lastSent >= 0) && (lastSent != SOCKET_ERROR)) {
            totalSent += static_cast<SIZE_T>(lastSent);

        } else {
            throw SocketException(__FILE__, __LINE__);
        }
#endif /* _WIN32 */

    } while (forceSend && (totalSent < cntBytes));

#ifdef _WIN32
    if (!::WSACloseEvent(overlapped.hEvent)) {
        throw SocketException(__FILE__, __LINE__);
    }
#endif /*_WIN32 */

    return totalSent;
}


/*
 * vislib::net::Socket::sendTo
 */
SIZE_T vislib::net::Socket::sendTo(const SocketAddress& toAddr, 
        const void *data, const SIZE_T cntBytes, const INT flags, 
        const bool forceSend) {
    SIZE_T totalSent = 0;       // # of bytes totally sent.      
    INT lastSent = 0;           // # of bytes sent during last send() call.
    sockaddr to = static_cast<sockaddr>(toAddr);
#ifdef _WIN32
    WSAOVERLAPPED overlapped;   // Overlap structure for asynchronous recv().
    WSABUF wsaBuf;              // Buffer for WSA output.
    DWORD errorCode = 0;        // WSA error during last operation.
    DWORD inOutFlags = flags;   // Flags for WSA.

    if ((overlapped.hEvent = ::WSACreateEvent()) == WSA_INVALID_EVENT) {
        throw SocketException(__FILE__, __LINE__);
    }

    wsaBuf.buf = const_cast<char *>(static_cast<const char *>(data));
    wsaBuf.len = static_cast<u_long>(cntBytes);
#endif /* _WIN32 */

    do {
#ifdef _WIN32
        if (::WSASendTo(this->handle, &wsaBuf, 1, reinterpret_cast<DWORD *>(
                &lastSent), flags, &to, sizeof(to), &overlapped, NULL) != 0) {
            if ((errorCode = ::WSAGetLastError()) != WSA_IO_PENDING) {
                ::WSACloseEvent(overlapped.hEvent);
                throw SocketException(errorCode, __FILE__, __LINE__);
            }
            TRACE(DEBUG_TRACE_LEVEL, "Overlapped socket I/O pending.\n");
            if (!::WSAGetOverlappedResult(this->handle, &overlapped, 
                    reinterpret_cast<DWORD *>(&lastSent), TRUE, &inOutFlags)) {
                ::WSACloseEvent(overlapped.hEvent);
                throw SocketException(__FILE__, __LINE__);
            }
        }
        totalSent += lastSent;
        wsaBuf.buf += lastSent;
        wsaBuf.len -= lastSent;

#else /* _WIN32 */
        lastSent = ::sendto(this->handle, static_cast<const char *>(data), 
            static_cast<int>(cntBytes - totalSent), flags, &to, sizeof(to));

        if ((lastSent >= 0) && (lastSent != SOCKET_ERROR)) {
            totalSent += static_cast<SIZE_T>(lastSent);

        } else {
            throw SocketException(__FILE__, __LINE__);
        }
#endif /* _WIN32 */

    } while (forceSend && (totalSent < cntBytes));

#ifdef _WIN32
    if (!::WSACloseEvent(overlapped.hEvent)) {
        throw SocketException(__FILE__, __LINE__);
    }
#endif /*_WIN32 */

    return totalSent;
}


/*
 * vislib::net::Socket::DEBUG_TRACE_LEVEL 
 */
const int vislib::net::Socket::DEBUG_TRACE_LEVEL = vislib::Trace::LEVEL_VL_INFO 
    + 1000;
