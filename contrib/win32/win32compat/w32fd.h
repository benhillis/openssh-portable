/*
* Author: Manoj Ampalam <manoj.ampalam@microsoft.com>
*
* Definitions for Win32 wrapper functions with POSIX like signatures
*
* Copyright (c) 2015 Microsoft Corp.
* All rights reserved
*
* Microsoft openssh win32 port
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
*
* 1. Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
* IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
* IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
* NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
* THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include <Windows.h>
#include <stdio.h>

enum w32_io_type {
	UNKNOWN_FD = 0,
	SOCK_FD = 1,	/*maps a socket fd*/
	NONSOCK_FD = 2,	/*maps a file fd, pipe fd or a tty fd*/
	STD_IO_FD = 5	/*maps a std fd - ex. STDIN_FILE*/
};

enum w32_io_sock_state {
	SOCK_INITIALIZED = 0,
	SOCK_BOUND = 1,		/*bind called on socket*/
	SOCK_LISTENING = 2,	/*listen called on socket*/
	SOCK_ACCEPTED = 3,	/*socket returned from accept()*/
	SOCK_CONNECTING = 4,	/*connect called on socket, connect is in progress*/
	SOCK_CONNECTED = 5	/*connect completed on socket*/
};

/*
* This structure encapsulates the state info needed to map a File Descriptor
* to Win32 Handle
*/
struct w32_io {
	OVERLAPPED read_overlapped;
	OVERLAPPED write_overlapped;
	struct {
		char *buf; /*internal read buffer*/
		DWORD buf_size;
		DWORD remaining; /*bytes in internal buffer remaining to be read by application*/
		DWORD completed; /*bytes in internal buffer already read by application*/
		BOOL pending;	 /*waiting on a read operation to complete*/
		DWORD error;	 /*error reported on async read or accept completion*/
	}read_details;
	struct {
		char *buf; /*internal write buffer*/
		DWORD buf_size;
		DWORD remaining; /*bytes in internal buffer remaining to be written to network*/
		DWORD completed; /*bytes in internal buffer already written to network*/
		BOOL pending;	 /*waiting on a write operation to complete*/
		DWORD error;	 /*error reported on async write or connect completion*/
	}write_details;

	/*index at which this object is stored in fd_table*/
	int table_index;
	enum w32_io_type type;		/*hanldle type*/
	DWORD fd_flags;			/*fd flags from POSIX*/
	DWORD fd_status_flags;		/*fd status flags from POSIX*/

	/*underlying w32 handle*/
	union {
		SOCKET sock;
		HANDLE handle;
		DWORD std_handle;  /* ex. STD_INPUT_HANDLE */
	};

	/*handle specific internal state context, used by sockets and pipes*/
	struct {
		enum w32_io_sock_state state;
		void* context;
	}internal;
};

#define WINHANDLE(pio) (((pio)->type == STD_IO_FD)? GetStdHandle((pio)->std_handle):(pio)->handle)
#define FILETYPE(pio) (GetFileType(WINHANDLE(pio)))
extern HANDLE main_thread;

BOOL w32_io_is_blocking(struct w32_io*);
BOOL w32_io_is_io_available(struct w32_io* pio, BOOL rd);
int wait_for_any_event(HANDLE* events, int num_events, DWORD milli_seconds);

/*POSIX mimic'ing socket API*/
int socketio_initialize();
int socketio_done();
BOOL socketio_is_io_available(struct w32_io* pio, BOOL rd);
void socketio_on_select(struct w32_io* pio, BOOL rd);
struct w32_io* socketio_socket(int domain, int type, int protocol);
struct w32_io* socketio_accept(struct w32_io* pio, struct sockaddr* addr, int* addrlen);
int socketio_setsockopt(struct w32_io* pio, int level, int optname, const char* optval, int optlen);
int socketio_getsockopt(struct w32_io* pio, int level, int optname, char* optval, int* optlen);
int socketio_getsockname(struct w32_io* pio, struct sockaddr* name, int* namelen);
int socketio_getpeername(struct w32_io* pio, struct sockaddr* name, int* namelen);
int socketio_listen(struct w32_io* pio, int backlog);
int socketio_bind(struct w32_io* pio, const struct sockaddr *name, int namelen);
int socketio_connect(struct w32_io* pio, const struct sockaddr* name, int namelen);
int socketio_finish_connect(struct w32_io* pio);
int socketio_recv(struct w32_io* pio, void *buf, size_t len, int flags);
int socketio_send(struct w32_io* pio, const void *buf, size_t len, int flags);
int socketio_shutdown(struct w32_io* pio, int how);
int socketio_close(struct w32_io* pio);

/*POSIX mimic'ing file API*/
BOOL fileio_is_io_available(struct w32_io* pio, BOOL rd);
void fileio_on_select(struct w32_io* pio, BOOL rd);
int fileio_close(struct w32_io* pio);
int fileio_pipe(struct w32_io* pio[2]);
struct w32_io* fileio_open(const char *pathname, int flags, int mode);
int fileio_read(struct w32_io* pio, void *dst, unsigned int max);
int fileio_write(struct w32_io* pio, const void *buf, unsigned int max);
int fileio_fstat(struct w32_io* pio, struct _stat64 *buf);
int fileio_stat(const char *path, struct _stat64 *buf);
long fileio_lseek(struct w32_io* pio, long offset, int origin);
FILE* fileio_fdopen(struct w32_io* pio, const char *mode);
/* AF_UNIX APIs*/
struct w32_io* fileio_afunix_socket();
int fileio_connect(struct w32_io*, char*);
int fileio_bind(struct w32_io*, char*);
int fileio_listen(struct w32_io*, int);
struct w32_io* fileio_accept(struct w32_io*);
/* terminal io specific versions */
int termio_close(struct w32_io* pio);

/*
* open() flags and modes
* all commented out macros are defined in fcntl.h
* they are listed here so as to cross check any conflicts with macros explicitly
* defined below.
*/
/*open access modes. only one of these can be specified*/
/* #define O_RDONLY    0x0  */
/* #define O_WRONLY    0x1 */
/* #define O_RDWR      0x2 */
/* open file creation and file status flags can be bitwise-or'd*/
/* #define O_APPEND    0x8    /*file is opened in append mode*/
#ifndef O_NONBLOCK
#define O_NONBLOCK  0x0004    /*io operations wont block*/
#endif
/* #define O_CREAT     0x100   /*If the file does not exist it will be created*/
/*
* If the file exists and is a regular file, and the file is successfully
* opened O_RDWR or O_WRONLY, its length shall be truncated to 0, and the mode
* and owner shall be unchanged
*/
/* #define O_TRUNC     0x200    */
/* If O_CREAT and O_EXCL are set, open() shall fail if the file exists */
/* #define O_EXCL      0x400   */
/* #define O_BINARY    0x8000   //Gives raw data (while O_TEXT normalises line endings */
/* open modes */
#ifndef  S_IRUSR
#define S_IRUSR     00400   /* user has read permission */
#endif /* ! S_IRUSR */
#ifndef S_IWUSR
#define S_IWUSR     00200   /* user has write permission */ 
#endif
#ifndef S_IRGRP
#define S_IRGRP     00040   /* group has read permission  */
#endif
#ifndef S_IROTH
#define S_IROTH     00004   /* others have read permission */
#endif