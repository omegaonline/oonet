///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2012 Rick Taylor
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

#ifndef OOSVRBASE_PROACTOR_POSIX_H_INCLUDED_
#define OOSVRBASE_PROACTOR_POSIX_H_INCLUDED_

#include "../include/OOBase/Proactor.h"
#include "../include/OOBase/Condition.h"
#include "../include/OOBase/Set.h"

#if defined(HAVE_UNISTD_H)

namespace OOBase
{
	namespace detail
	{
		enum TxDirection
		{
			eTXRecv = 1,
			eTXSend = 2
		};

		class ProactorPosix : public Proactor
		{
		// Proactor public members
		public:
			Acceptor* accept(void* param, accept_pipe_callback_t callback, const char* path, int& err, SECURITY_ATTRIBUTES* psa);
			Acceptor* accept(void* param, accept_callback_t callback, const sockaddr* addr, socklen_t addr_len, int& err);

			AsyncSocket* attach(socket_t sock, int& err);

			AsyncSocket* connect(const sockaddr* addr, socklen_t addr_len, int& err, const Timeout& timeout);
			AsyncSocket* connect(const char* path, int& err, const Timeout& timeout);

		// 'Internal' public members
		public:
			typedef void (*fd_callback_t)(int fd, void* param, unsigned int events);

			int bind_fd(int fd, void* param, fd_callback_t callback);
			int unbind_fd(int fd);

			int watch_fd(int fd, unsigned int events);

			typedef Timeout (*timer_callback_t)(void* param);

			int start_timer(void* param, timer_callback_t callback, const Timeout& timeout);
			int stop_timer(void* param);

			void stop();
			int restart();

			AllocatorInstance& get_internal_allocator()
			{
				return m_allocator;
			}

		protected:
			struct TimerItem
			{
				void*            m_param;
				timer_callback_t m_callback;
				Timeout          m_timeout;

				bool operator > (const TimerItem& rhs) const
				{
					return m_timeout > rhs.m_timeout;
				}

				bool operator == (const TimerItem& rhs) const
				{
					return m_timeout == rhs.m_timeout;
				}
			};

			ProactorPosix();
			virtual ~ProactorPosix();

			int init();

			int read_control();

			bool check_timers(TimerItem& active_timer, Timeout& timeout);
			int process_timer(const TimerItem& active_timer);

			virtual bool do_bind_fd(int fd, void* param, fd_callback_t callback) = 0;
			virtual bool do_watch_fd(int fd, unsigned int events) = 0;
			virtual bool do_unbind_fd(int fd) = 0;

			Mutex                 m_lock;
			LockedAllocator<4096> m_allocator;
			bool                  m_stopped;
			int                   m_read_fd;

		private:
			Set<TimerItem,Greater<TimerItem>,AllocatorInstance> m_timers;
			int                                                 m_write_fd;

			bool add_timer(void* param, timer_callback_t callback, const Timeout& timeout);
			bool remove_timer(void* param);
			int watch_fd_i(int fd, unsigned int events, Future<int>* future);
		};
	}
}

#endif // defined(HAVE_UNISTD_H) && !defined(_WIN32)

#endif // OOSVRBASE_PROACTOR_POSIX_H_INCLUDED_
