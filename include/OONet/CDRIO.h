///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2013 Rick Taylor
//
// This file is part of OOBase, the Omega Online Base library.
//
// OOBase is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// OOBase is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with OOBase.  If not, see <http://www.gnu.org/licenses/>.
//
///////////////////////////////////////////////////////////////////////////////////

#ifndef OOBASE_CDR_IO_H_INCLUDED_
#define OOBASE_CDR_IO_H_INCLUDED_

#include "Proactor.h"
#include "CDRStream.h"

namespace OOBase
{
	// This is a class purely to allow friends
	class CDRIO
	{
	public:
		template <typename H, typename S>
		static int recv_with_header_blocking(CDRStream& stream, S pSocket)
		{
			int err = pSocket->recv(stream.buffer(),sizeof(H));
			if (!err)
			{
				H msg_len = 0;
				if (!stream.read(msg_len))
					err = stream.last_error();
				else if (msg_len > sizeof(H))
					err = pSocket->recv(stream.buffer(),msg_len - sizeof(H));
			}
			return err;
		}

		template <typename H, typename T>
		static int recv_with_header_sync(H expected_len, AsyncSocket* pSocket, T* param, void (T::*callback)(CDRStream& stream, int err))
		{
			CDRStream stream(expected_len);
			if (stream.last_error())
				return stream.last_error();

			ThunkRHS<T,H>* thunk = NULL;
			if (!pSocket->thunk_allocate(thunk,param,callback))
				return ERROR_OUTOFMEMORY;

			thunk->m_ptrSocket = pSocket;
			thunk->m_ptrSocket->addref();
			return pSocket->recv(thunk,&ThunkRHS<T,H>::fn1,stream.buffer(),sizeof(H));
		}

		template <typename H, typename S>
		static int recv_msg_with_header_blocking(CDRStream& stream, const RefPtr<Buffer>& ctl_buffer, S pSocket)
		{
			int err = pSocket->recv_msg(stream.buffer(),ctl_buffer,sizeof(H));
			if (!err)
			{
				H msg_len = 0;
				if (!stream.read(msg_len))
					err = stream.last_error();
				else if (msg_len > sizeof(H))
					err = pSocket->recv(stream.buffer(),msg_len - sizeof(H));
			}
			return err;
		}

		template <typename H, typename T>
		static int recv_msg_with_header_sync(H expected_len, AsyncSocket* pSocket, T* param, void (T::*callback)(CDRStream& stream, const RefPtr<Buffer>& ctl_buffer, int err), const RefPtr<Buffer>& ctl_buffer)
		{
			CDRStream stream(expected_len);
			if (stream.last_error())
				return stream.last_error();

			ThunkRMHS<T,H>* thunk = NULL;
			if (!pSocket->thunk_allocate(thunk,param,callback))
				return ERROR_OUTOFMEMORY;

			thunk->m_ptrSocket = pSocket;
			thunk->m_ptrSocket->addref();
			return pSocket->recv_msg(thunk,&ThunkRMHS<T,H>::fn1,stream.buffer(),ctl_buffer,sizeof(H));
		}

		template <typename H, typename S>
		static int send_and_recv_with_header_blocking(CDRStream& stream, S pSocket)
		{
			int err = pSocket->send(stream.buffer());
			if (!err)
			{
				stream.reset();
				err = pSocket->recv(stream.buffer(),sizeof(H));
				if (!err)
				{
					H msg_len = 0;
					if (!stream.read(msg_len))
						err = stream.last_error();
					else if (msg_len > sizeof(H))
						err = pSocket->recv(stream.buffer(),msg_len - sizeof(H));
				}
			}
			return err;
		}

		template <typename H, typename S>
		static int send_and_recv_msg_with_header_blocking(CDRStream& stream, const RefPtr<Buffer>& ctl_buffer, S pSocket)
		{
			int err = pSocket->send(stream.buffer());
			if (!err)
			{
				stream.reset();
				err = pSocket->recv_msg(stream.buffer(),ctl_buffer,sizeof(H));
				if (!err)
				{
					H msg_len = 0;
					if (!stream.read(msg_len))
						err = stream.last_error();
					else if (msg_len > sizeof(H))
						err = pSocket->recv(stream.buffer(),msg_len - sizeof(H));
				}
			}
			return err;
		}

		template <typename H, typename S>
		static int send_msg_and_recv_with_header_blocking(CDRStream& stream, const RefPtr<Buffer>& ctl_buffer, S pSocket)
		{
			int err = pSocket->send_msg(stream.buffer(),ctl_buffer);
			if (!err)
			{
				stream.reset();
				err = pSocket->recv(stream.buffer(),sizeof(H));
				if (!err)
				{
					H msg_len = 0;
					if (!stream.read(msg_len))
						err = stream.last_error();
					else if (msg_len > sizeof(H))
						err = pSocket->recv(stream.buffer(),msg_len - sizeof(H));
				}
			}
			return err;
		}

		template <typename H, typename S>
		static int send_msg_and_recv_msg_with_header_blocking(CDRStream& stream, const RefPtr<Buffer>& ctl_buffer, S pSocket)
		{
			int err = pSocket->send_msg(stream.buffer(),ctl_buffer);
			if (!err)
			{
				stream.reset();
				ctl_buffer->reset();
				err = pSocket->recv_msg(stream.buffer(),ctl_buffer,sizeof(H));
				if (!err)
				{
					H msg_len = 0;
					if (!stream.read(msg_len))
						err = stream.last_error();
					else if (msg_len > sizeof(H))
						err = pSocket->recv(stream.buffer(),msg_len - sizeof(H));
				}
			}
			return err;
		}

		template <typename H, typename T>
		static int send_and_recv_with_header_sync(CDRStream& stream, AsyncSocket* pSocket, T* param, void (T::*callback)(CDRStream& stream, int err))
		{
			ThunkSRHS<T,H>* thunk = NULL;
			if (!pSocket->thunk_allocate(thunk,param,callback))
				return ERROR_OUTOFMEMORY;

			thunk->m_ptrSocket = pSocket;
			thunk->m_ptrSocket->addref();
			return pSocket->send(thunk,&ThunkSRHS<T,H>::fn1,stream.buffer());
		}

	private:
		template <typename T, typename H>
		struct ThunkRHS
		{
			ThunkRHS(T* param, void (T::*callback)(CDRStream&,int), AllocatorInstance& allocator) :
				m_param(param),m_callback(callback),m_allocator(allocator)
			{}

			ThunkRHS(const ThunkRHS& rhs) : m_param(rhs.m_param), m_callback(rhs.m_callback), m_allocator(rhs.m_allocator), m_ptrSocket(rhs.m_ptrSocket)
			{}

			ThunkRHS& operator = (const ThunkRHS& rhs)
			{
				if (this != &rhs)
				{
					m_param = rhs.m_param;
					m_callback = rhs.m_callback;
					m_allocator = rhs.m_allocator;
					m_ptrSocket = rhs.m_ptrSocket;
				}
				return *this;
			}

			T* m_param;
			void (T::*m_callback)(CDRStream&,int);
			AllocatorInstance&  m_allocator;
			RefPtr<AsyncSocket> m_ptrSocket;

			static void fn1(void* param, const RefPtr<Buffer>& buffer, int err)
			{
				CDRStream stream(buffer);
				bool done = false;
				if (!err)
				{
					H msg_len = 0;
					if (!stream.read(msg_len))
						err = stream.last_error();
					else if (msg_len > sizeof(H))
						err = static_cast<ThunkRHS*>(param)->m_ptrSocket->recv(param,&fn2,buffer,msg_len - sizeof(H));
					else
						done = true;
				}
				if (err)
					done = true;
				if (done)
				{
					ThunkRHS thunk = *static_cast<ThunkRHS*>(param);
					thunk.m_allocator.delete_free(static_cast<ThunkRHS*>(param));
					(thunk.m_param->*thunk.m_callback)(stream,err);
				}
			}

			static void fn2(void* param, const RefPtr<Buffer>& buffer, int err)
			{
				ThunkRHS thunk = *static_cast<ThunkRHS*>(param);
				thunk.m_allocator.delete_free(static_cast<ThunkRHS*>(param));
				CDRStream stream(buffer);
				(thunk.m_param->*thunk.m_callback)(stream,err);
			}
		};

		template <typename T, typename H>
		struct ThunkRMHS
		{
			ThunkRMHS(T* param, void (T::*callback)(CDRStream&,const RefPtr<Buffer>&,int), AllocatorInstance& allocator) :
				m_param(param),m_callback(callback),m_allocator(allocator)
			{}

			ThunkRMHS(const ThunkRMHS& rhs) : m_param(rhs.m_param), m_callback(rhs.m_callback), m_allocator(rhs.m_allocator), m_ptrSocket(rhs.m_ptrSocket), m_ctl_buffer(rhs.m_ctl_buffer)
			{}

			ThunkRMHS& operator = (const ThunkRMHS& rhs)
			{
				if (this != &rhs)
				{
					m_param = rhs.m_param;
					m_callback = rhs.m_callback;
					m_allocator = rhs.m_allocator;
					m_ptrSocket = rhs.m_ptrSocket;
					m_ctl_buffer = rhs.m_ctl_buffer;
				}
				return *this;
			}

			T* m_param;
			void (T::*m_callback)(CDRStream&,const RefPtr<Buffer>&,int);
			AllocatorInstance&  m_allocator;
			RefPtr<AsyncSocket> m_ptrSocket;
			RefPtr<Buffer>      m_ctl_buffer;

			static void fn1(void* param, const RefPtr<Buffer>& data_buffer, const RefPtr<Buffer>& ctl_buffer, int err)
			{
				CDRStream stream(data_buffer);
				bool done = false;
				if (!err)
				{
					H msg_len = 0;
					if (!stream.read(msg_len))
						err = stream.last_error();
					else
					{
						static_cast<ThunkRMHS*>(param)->m_ctl_buffer = ctl_buffer;
						ctl_buffer->addref();

						if (msg_len > sizeof(H))
							err = static_cast<ThunkRMHS*>(param)->m_ptrSocket->recv(param,&fn2,data_buffer,msg_len - sizeof(H));
						else
							done = true;
					}
				}
				if (err)
					done = true;
				if (done)
				{
					ThunkRMHS thunk = *static_cast<ThunkRMHS*>(param);
					thunk.m_allocator.delete_free(static_cast<ThunkRMHS*>(param));
					(thunk.m_param->*thunk.m_callback)(stream,ctl_buffer,err);
				}
			}

			static void fn2(void* param, const RefPtr<Buffer>& data_buffer, int err)
			{
				ThunkRMHS thunk = *static_cast<ThunkRMHS*>(param);
				thunk.m_allocator.delete_free(static_cast<ThunkRMHS*>(param));
				CDRStream stream(data_buffer);
				(thunk.m_param->*thunk.m_callback)(stream,thunk.m_ctl_buffer,err);
			}
		};

		template <typename T, typename H>
		struct ThunkSRHS
		{
			ThunkSRHS(T* param, void (T::*callback)(CDRStream&,int), AllocatorInstance& allocator) :
				m_param(param),m_callback(callback),m_allocator(allocator)
			{}

			ThunkSRHS(const ThunkSRHS& rhs) : m_param(rhs.m_param), m_callback(rhs.m_callback), m_allocator(rhs.m_allocator), m_ptrSocket(rhs.m_ptrSocket)
			{}

			ThunkSRHS& operator = (const ThunkSRHS& rhs)
			{
				if (this != &rhs)
				{
					m_param = rhs.m_param;
					m_callback = rhs.m_callback;
					m_allocator = rhs.m_allocator;
					m_ptrSocket = rhs.m_ptrSocket;
				}
				return *this;
			}

			T* m_param;
			void (T::*m_callback)(CDRStream&,int);
			AllocatorInstance&  m_allocator;
			RefPtr<AsyncSocket> m_ptrSocket;

			static void fn1(void* param, const RefPtr<Buffer>& buffer, int err)
			{
				CDRStream stream(buffer);
				if (!err)
				{
					err = stream.reset();
					if (!err)
						err = static_cast<ThunkSRHS*>(param)->m_ptrSocket->recv(param,&fn2,buffer,sizeof(H));
				}
				if (err)
				{
					ThunkSRHS thunk = *static_cast<ThunkSRHS*>(param);
					thunk.m_allocator.delete_free(static_cast<ThunkSRHS*>(param));
					(thunk.m_param->*thunk.m_callback)(stream,err);
				}
			}

			static void fn2(void* param, const RefPtr<Buffer>& buffer, int err)
			{
				CDRStream stream(buffer);
				bool done = false;
				if (!err)
				{
					H msg_len = 0;
					if (!stream.read(msg_len))
						err = stream.last_error();
					else if (msg_len > sizeof(H))
						err = static_cast<ThunkSRHS*>(param)->m_ptrSocket->recv(param,&fn3,buffer,msg_len - sizeof(H));
					else
						done = true;
				}
				if (err)
					done = true;
				if (done)
				{
					ThunkSRHS thunk = *static_cast<ThunkSRHS*>(param);
					thunk.m_allocator.delete_free(static_cast<ThunkSRHS*>(param));
					(thunk.m_param->*thunk.m_callback)(stream,err);
				}
			}

			static void fn3(void* param, const RefPtr<Buffer>& buffer, int err)
			{
				ThunkSRHS thunk = *static_cast<ThunkSRHS*>(param);
				thunk.m_allocator.delete_free(static_cast<ThunkSRHS*>(param));
				CDRStream stream(buffer);
				(thunk.m_param->*thunk.m_callback)(stream,err);
			}
		};
	};
}

#endif // OOBASE_CDR_IO_H_INCLUDED_
