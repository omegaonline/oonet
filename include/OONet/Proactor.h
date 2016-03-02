///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2009 Rick Taylor
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

#ifndef OOSVRBASE_PROACTOR_H_INCLUDED_
#define OOSVRBASE_PROACTOR_H_INCLUDED_

#include "../OOBase/Memory.h"
#include "../OOBase/Socket.h"
#include "../OOBase/Singleton.h"

#if !defined(_WIN32)
typedef struct
{
	mode_t mode;
	bool   pass_credentials;
} SECURITY_ATTRIBUTES;
#endif

namespace OOBase
{
	class AsyncSocket : public RefCounted
	{
		friend class CDRIO;

	public:
		template <typename T>
		int recv(T* param, void (T::*callback)(const RefPtr<Buffer>& buffer, int err), const RefPtr<Buffer>& buffer, size_t bytes = 0)
		{
			Thunk<T>* thunk = NULL;
			if (!thunk_allocate(thunk,param,callback))
				return ERROR_OUTOFMEMORY;
			
			return recv(thunk,&Thunk<T>::fn,buffer,bytes);
		}

		template <typename T>
		int recv_msg(T* param, void (T::*callback)(const RefPtr<Buffer>& data_buffer, const RefPtr<Buffer>& ctl_buffer, int err), const RefPtr<Buffer>& data_buffer, const RefPtr<Buffer>& ctl_buffer, size_t data_bytes)
		{
			ThunkM<T>* thunk = NULL;
			if (!thunk_allocate(thunk,param,callback))
				return ERROR_OUTOFMEMORY;

			return recv_msg(thunk,&ThunkM<T>::fn,data_buffer,ctl_buffer,data_bytes);
		}

		template <typename T>
		int send(T* param, void (T::*callback)(const RefPtr<Buffer>& buffer, int err), const RefPtr<Buffer>& buffer)
		{
			Thunk<T>* thunk = NULL;
			if (!thunk_allocate(thunk,param,callback))
				return ERROR_OUTOFMEMORY;
			
			return send(thunk,&Thunk<T>::fn,buffer);
		}

		template <typename T>
		int send_v(T* param, void (T::*callback)(Buffer* buffers[], size_t count, int err), Buffer* buffers[], size_t count)
		{
			ThunkV<T>* thunk = NULL;
			if (!thunk_allocate(thunk,param,callback))
				return ERROR_OUTOFMEMORY;

			return send_v(thunk,&ThunkV<T>::fn,buffers,count);
		}

		template <typename T>
		int send_msg(T* param, void (T::*callback)(const RefPtr<Buffer>& data_buffer, const RefPtr<Buffer>& ctl_buffer, int err), const RefPtr<Buffer>& data_buffer, const RefPtr<Buffer>& ctl_buffer)
		{
			ThunkM<T>* thunk = NULL;
			if (!thunk_allocate(thunk,param,callback))
				return ERROR_OUTOFMEMORY;
			
			return send_msg(thunk,&ThunkM<T>::fn,data_buffer,ctl_buffer);
		}

		// These are blocking calls
		int recv(const RefPtr<Buffer>& buffer, size_t bytes = 0);
		int recv_msg(const RefPtr<Buffer>& buffer, const RefPtr<Buffer>& ctl_buffer, size_t bytes);
		int send(const RefPtr<Buffer>& buffer);
		int send_msg(const RefPtr<Buffer>& buffer, const RefPtr<Buffer>& ctl_buffer);

		typedef void (*recv_callback_t)(void* param, const RefPtr<Buffer>& buffer, int err);
		virtual int recv(void* param, recv_callback_t callback, const RefPtr<Buffer>& buffer, size_t bytes = 0) = 0;

		typedef void (*recv_msg_callback_t)(void* param, const RefPtr<Buffer>& data_buffer, const RefPtr<Buffer>& ctl_buffer, int err);
		virtual int recv_msg(void* param, recv_msg_callback_t callback, const RefPtr<Buffer>& data_buffer, const RefPtr<Buffer>& ctl_buffer, size_t data_bytes) = 0;

		typedef void (*send_callback_t)(void* param, const RefPtr<Buffer>& buffer, int err);
		virtual int send(void* param, send_callback_t callback, const RefPtr<Buffer>& buffer) = 0;

		typedef void (*send_v_callback_t)(void* param, Buffer* buffers[], size_t count, int err);
		virtual int send_v(void* param, send_v_callback_t callback, Buffer* buffers[], size_t count) = 0;

		typedef void (*send_msg_callback_t)(void* param, const RefPtr<Buffer>& data_buffer, const RefPtr<Buffer>& ctl_buffer, int err);
		virtual int send_msg(void* param, send_msg_callback_t callback, const RefPtr<Buffer>& data_buffer, const RefPtr<Buffer>& ctl_buffer) = 0;

		virtual int shutdown(bool bSend = true, bool bRecv = true) = 0;

		virtual socket_t get_handle() const = 0;

	protected:
		AsyncSocket() {}
		virtual ~AsyncSocket() {}

		template <typename TThunk, typename TP, typename TC>
		bool thunk_allocate(TThunk*& t, TP param, TC callback)
		{
			t = NULL;
			AllocatorInstance& allocator = get_internal_allocator();
			return allocator.allocate_new(t,param,callback,allocator);
		}

		virtual AllocatorInstance& get_internal_allocator() const = 0;

	private:
		template <typename T>
		struct Thunk
		{
			Thunk(T* param, void (T::*callback)(const RefPtr<Buffer>&,int), AllocatorInstance& allocator) :
				m_param(param),m_callback(callback),m_allocator(allocator)
			{}

			Thunk(const Thunk& rhs) : m_param(rhs.m_param), m_callback(rhs.m_callback), m_allocator(rhs.m_allocator)
			{}

			Thunk& operator = (const Thunk& rhs)
			{
				if (this != &rhs)
				{
					m_param = rhs.m_param;
					m_callback = rhs.m_callback;
					m_allocator = rhs.m_allocator;
				}
				return *this;
			}

			T* m_param;
			void (T::*m_callback)(const RefPtr<Buffer>&,int);
			AllocatorInstance& m_allocator;

			static void fn(void* param, const RefPtr<Buffer>& buffer, int err)
			{
				Thunk thunk = *static_cast<Thunk*>(param);
				thunk.m_allocator.delete_free(static_cast<Thunk*>(param));
				(thunk.m_param->*thunk.m_callback)(buffer,err);
			}
		};
		
		template <typename T>
		struct ThunkM
		{
			ThunkM(T* param, void (T::*callback)(const RefPtr<Buffer>&,const RefPtr<Buffer>&,int), AllocatorInstance& allocator) :
				m_param(param),m_callback(callback),m_allocator(allocator)
			{}

			ThunkM(const ThunkM& rhs) : m_param(rhs.m_param), m_callback(rhs.m_callback), m_allocator(rhs.m_allocator)
			{}

			ThunkM& operator = (const ThunkM& rhs)
			{
				if (this != &rhs)
				{
					m_param = rhs.m_param;
					m_callback = rhs.m_callback;
					m_allocator = rhs.m_allocator;
				}
				return *this;
			}

			T* m_param;
			void (T::*m_callback)(const RefPtr<Buffer>&,const RefPtr<Buffer>&,int);
			AllocatorInstance& m_allocator;

			static void fn(void* param, const RefPtr<Buffer>& data_buffer, const RefPtr<Buffer>& ctl_buffer, int err)
			{
				ThunkM thunk = *static_cast<ThunkM*>(param);
				thunk.m_allocator.delete_free(static_cast<ThunkM*>(param));
				(thunk.m_param->*thunk.m_callback)(data_buffer,ctl_buffer,err);
			}
		};

		template <typename T>
		struct ThunkV
		{
			ThunkV(T* param, void (T::*callback)(Buffer*[],size_t,int), AllocatorInstance& allocator) :
				m_param(param),m_callback(callback),m_allocator(allocator)
			{}

			ThunkV(const ThunkV& rhs) : m_param(rhs.m_param), m_callback(rhs.m_callback), m_allocator(rhs.m_allocator)
			{}

			ThunkV& operator = (const ThunkV& rhs)
			{
				if (this != &rhs)
				{
					m_param = rhs.m_param;
					m_callback = rhs.m_callback;
					m_allocator = rhs.m_allocator;
				}
				return *this;
			}

			T* m_param;
			void (T::*m_callback)(Buffer*[],size_t,int);
			AllocatorInstance& m_allocator;

			static void fn(void* param, Buffer* buffers[], size_t count, int err)
			{
				ThunkV thunk = *static_cast<ThunkV*>(param);
				thunk.m_allocator.delete_free(static_cast<ThunkV*>(param));
				(thunk.m_param->*thunk.m_callback)(buffers,count,err);
			}
		};
	};

	class Acceptor : public RefCounted
	{
	public:
		// No members, just release() to close
			
	protected:
		Acceptor() {}
		virtual ~Acceptor() {}
	};

	class Proactor : public NonCopyable
	{
	public:
		// Factory creation functions
		static Proactor* create(int& err);
		static void destroy(Proactor* proactor);

		typedef void (*accept_pipe_callback_t)(void* param, AsyncSocket* pSocket, int err);
		virtual Acceptor* accept(void* param, accept_pipe_callback_t callback, const char* path, int& err, SECURITY_ATTRIBUTES* psa = NULL) = 0;

		typedef void (*accept_callback_t)(void* param, AsyncSocket* pSocket, const sockaddr* addr, socklen_t addr_len, int err);
		virtual Acceptor* accept(void* param, accept_callback_t callback, const sockaddr* addr, socklen_t addr_len, int& err) = 0;

		virtual AsyncSocket* attach(socket_t sock, int& err) = 0;
#if defined(_WIN32)
		virtual AsyncSocket* attach(HANDLE hPipe, int& err) = 0;

		virtual Acceptor* accept_unique_pipe(void* param, accept_pipe_callback_t callback, /*(out)*/ char path[64], int& err, SECURITY_ATTRIBUTES* psa = NULL) = 0;
		Acceptor* accept_unique_pipe(void* param, accept_pipe_callback_t callback, /*(out)*/ char path[64], int& err, const char* pszSID);

		typedef void (*wait_object_callback_t)(void* param, HANDLE hObject, bool bTimedout, int err);
		virtual Acceptor* wait_for_object(void* param, wait_object_callback_t callback, HANDLE hObject, int& err, ULONG dwMilliseconds = INFINITE) = 0;
#endif

		virtual AsyncSocket* connect(const sockaddr* addr, socklen_t addr_len, int& err, const Timeout& timeout) = 0;
		virtual AsyncSocket* connect(const char* path, int& err, const Timeout& timeout) = 0;

		// Returns -1 on error, 0 on timeout, 1 on nothing more to do
		virtual int run(int& err, const Timeout& timeout = Timeout()) = 0;
		virtual void stop() = 0;
		virtual int restart() = 0;

	protected:
		Proactor() {}
		virtual ~Proactor() {}
	};

	template <typename LibraryType>
	class Singleton<Proactor,LibraryType> : public NonCopyable
	{
	public:
		static Proactor* instance_ptr()
		{
			static Once::once_t key = ONCE_T_INIT;
			Once::Run(&key,init);

			return s_instance;
		}

		static Proactor& instance()
		{
			Proactor* i = instance_ptr();
			if (!i)
				OOBase_CallCriticalFailure("Null instance pointer");

			return *i;
		}

	private:
		static Proactor* s_instance;

		static void init()
		{
			int err = 0;
			Proactor* i = Proactor::create(err);
			if (err)
				OOBase_CallCriticalFailure(err);

			err = DLLDestructor<LibraryType>::add_destructor(&destroy,i);
			if (err)
			{
				Proactor::destroy(i);
				OOBase_CallCriticalFailure(err);
			}

			s_instance = i;
		}

		static void destroy(void* i)
		{
			if (i == s_instance)
			{
				Proactor::destroy(static_cast<Proactor*>(i));
				s_instance = NULL;
			}
		}
	};

	template <typename LibraryType>
	Proactor* Singleton<Proactor,LibraryType>::s_instance = NULL;
}

#endif // OOSVRBASE_PROACTOR_H_INCLUDED_
