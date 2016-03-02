///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2010 Rick Taylor
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

#include "../include/OOBase/Server.h"
#include "../include/OOBase/Logger.h"
#include "../include/OOBase/Singleton.h"
#include "../include/OOBase/String.h"
#include "../include/OOBase/Posix.h"

#include "Win32Impl.h"

#if defined(HAVE_UNISTD_H)
#include <fcntl.h>
#include <stdlib.h>
#include <limits.h>
#endif

#if defined(_WIN32)

namespace
{
	struct QuitData : public OOBase::Future<int>
	{
		QuitData();

		void set_pid_event(HANDLE h);

	private:
		OOBase::Win32::SmartHandle m_pid_event;
	};
	typedef OOBase::Singleton<QuitData,OOBase::Module> QUIT;

	BOOL WINAPI control_c(DWORD evt)
	{
		QUIT::instance().signal(evt);
		return TRUE;
	}

	QuitData::QuitData() : OOBase::Future<int>(-1)
	{
		if (!SetConsoleCtrlHandler(control_c,TRUE))
			LOG_ERROR(("SetConsoleCtrlHandler failed: %s",OOBase::system_error_text()));
	}

	void QuitData::set_pid_event(HANDLE h)
	{
		m_pid_event = h;
	}

	static SERVICE_STATUS_HANDLE s_ssh = (SERVICE_STATUS_HANDLE)NULL;
	static bool s_daemonized = false;

	VOID WINAPI ServiceCtrl(DWORD dwControl)
	{
		SERVICE_STATUS ss = {0};
		ss.dwControlsAccepted = SERVICE_ACCEPT_STOP|SERVICE_ACCEPT_SHUTDOWN;
		ss.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
		ss.dwCurrentState = SERVICE_RUNNING;

		switch (dwControl)
		{
		case SERVICE_CONTROL_STOP:
		case SERVICE_CONTROL_SHUTDOWN:
			ss.dwCurrentState = SERVICE_STOP_PENDING;
			break;

		default:
			break;
		}

		// Set the status of the service
		if (s_ssh)
			SetServiceStatus(s_ssh,&ss);

		// Tell service_main thread to stop.
		switch (dwControl)
		{
		case SERVICE_CONTROL_STOP:
			QUIT::instance().signal(CTRL_CLOSE_EVENT);
			break;

		case SERVICE_CONTROL_SHUTDOWN:
			QUIT::instance().signal(CTRL_SHUTDOWN_EVENT);
			break;

		default:
			break;
		}
	}

	VOID WINAPI ServiceMain(DWORD /*dwArgc*/, LPWSTR* lpszArgv)
	{
		SERVICE_STATUS ss = {0};
		ss.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_PARAMCHANGE;
		ss.dwServiceType = SERVICE_WIN32_OWN_PROCESS;

		// Register the service ctrl handler.
		s_ssh = RegisterServiceCtrlHandlerW(lpszArgv[0],&ServiceCtrl);
		if (s_ssh)
		{
			ss.dwCurrentState = SERVICE_RUNNING;
			SetServiceStatus(s_ssh,&ss);
		}

		// Wait for the event to be signalled
		QUIT::instance().wait(false);

		if (s_ssh)
		{
			ss.dwCurrentState = SERVICE_STOPPED;
			SetServiceStatus(s_ssh,&ss);
		}
	}
}

template class OOBase::Singleton<QuitData,OOBase::Module>;

OOBase::Server::Server()
{
}

void OOBase::Server::signal(int how)
{
	QUIT::instance().signal(how);
}

void OOBase::Server::quit()
{
	signal(CTRL_C_EVENT);
}

int OOBase::Server::wait_for_quit()
{
	if (s_daemonized)
	{
		static wchar_t sn[] = L"";
		static SERVICE_TABLE_ENTRYW ste[] =
		{
			{sn, &ServiceMain },
			{NULL, NULL}
		};

		// Because it can take a while to determine if we are a service or not,
		// install the Ctrl+C handlers first
		QUIT::instance();

		if (!StartServiceCtrlDispatcherW(ste))
		{
			DWORD err = GetLastError();
			if (err != ERROR_FAILED_SERVICE_CONTROLLER_CONNECT)
				return GetLastError();
		}
	}

	// By the time we get here, it's all over
	return QUIT::instance().wait(false);
}

int OOBase::Server::create_pid_file(const char* szPidFile, bool& already)
{
	// Create a global event named pszPidFile
	// If it exists, then we are already running

	ScopedString strFullPidFile;
	int err = strFullPidFile.printf("Global\\%s",szPidFile);
	if (err)
		return err;

	for (size_t i=0;i<strFullPidFile.length();++i)
	{
		if (strFullPidFile[i] == '/')
			strFullPidFile[i] = '_';
	}

	OOBase::Win32::SmartHandle e(CreateEvent(NULL,TRUE,FALSE,strFullPidFile.c_str()));
	if (!e.valid())
	{
		err = GetLastError();
		if (err == ERROR_ALREADY_EXISTS)
		{
			already = true;
			err = 0;
		}
		return err;
	}

	// Ensure it's closed
	QUIT::instance().set_pid_event(e.detach());

	already = false;
	return 0;
}

int OOBase::Server::daemonize(const char* szPidFile, bool& already)
{
	int err = create_pid_file(szPidFile,already);
	if (!err)
		s_daemonized = true;
	return err;
}

#elif defined(HAVE_UNISTD_H)

namespace
{
	struct QuitData
	{
		~QuitData();

		void set_pid_file(const OOBase::String& strPidFile, int fd);

	private:
		OOBase::POSIX::SmartFD m_fd;
		OOBase::String         m_strPidFile;
	};
	typedef OOBase::Singleton<QuitData,OOBase::Module> QUIT;

	QuitData::~QuitData()
	{
		if (!m_strPidFile.empty())
			::unlink(m_strPidFile.c_str());
	}

	void QuitData::set_pid_file(const OOBase::String& strPidFile, int fd)
	{
		m_fd = fd;
		m_strPidFile = strPidFile;
	}

	int concat_pidname(OOBase::String& strPidFile, const char* pszPidFile)
	{
		if (!pszPidFile)
			return EINVAL;

		int err = 0;
		if (*pszPidFile != '/')
		{
			// Relative path, prepend cwd
			char* cwd = get_current_dir_name();
			if (!cwd)
				return errno;

			if (!strPidFile.assign(cwd))
				err = ERROR_OUTOFMEMORY;

			::free(cwd);

			if (!err && strPidFile[strPidFile.length()-1] != '/' && !strPidFile.append("/",1))
				err = ERROR_OUTOFMEMORY;

			if (!err && !strPidFile.append(pszPidFile))
				err = ERROR_OUTOFMEMORY;
		}
		else if (!strPidFile.assign(pszPidFile))
			err = ERROR_OUTOFMEMORY;

		return err;
	}

	int lock_pidfile(const OOBase::String& strPidFile, int& fd, bool& already)
	{
		fd = OOBase::POSIX::open(strPidFile.c_str(), O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP);
		if (fd == -1)
			return errno;

		// Try for the lock
		int err = 0;
		if (lockf(fd,F_TLOCK,0) == -1)
		{
			if (errno == EACCES || errno == EAGAIN)
			{
				already = true;
				OOBase::POSIX::close(fd);
				return 0;
			}
			err = errno;
		}
		else
		{
			already = false;

			// Make sure the file gets unlinked at the end
			QUIT::instance().set_pid_file(strPidFile,fd);

			// Write our pid to the file
			OOBase::ScopedString str;
			err = str.printf("%d",getpid());
			if (!err)
			{
				do
				{
					if (ftruncate(fd,0) == -1)
						err = errno;
				}
				while (err == EINTR);

				if (!err && OOBase::POSIX::write(fd,str.c_str(),str.length()) == -1)
					err = errno;
			}
		}

		if (err)
			OOBase::POSIX::close(fd);

		return err;
	}
}

template class OOBase::Singleton<QuitData,OOBase::Module>;

OOBase::Server::Server() :
		m_tid(pthread_self())
{
	sigemptyset(&m_set);
	sigaddset(&m_set, SIGQUIT);
	sigaddset(&m_set, SIGTERM);
	
	int err = pthread_sigmask(SIG_BLOCK, &m_set, NULL);
	if (err != 0)
		LOG_ERROR(("pthread_sigmask failed: %s",system_error_text(err)));
}

int OOBase::Server::wait_for_quit()
{
	int ret = 0;
	m_tid = pthread_self();
	sigwait(&m_set,&ret);
	return ret;
}

void OOBase::Server::signal(int sig)
{
	pthread_kill(m_tid,sig);
}

void OOBase::Server::quit()
{
	signal(SIGQUIT);
}

int OOBase::Server::create_pid_file(const char* szPidFile, bool& already)
{
	String strFullPidFile;
	int err = concat_pidname(strFullPidFile,szPidFile);
	if (err)
		return err;

	int fd;
	return lock_pidfile(strFullPidFile,fd,already);
}

int OOBase::Server::daemonize(const char* szPidFile, bool& already)
{
	String strFullPidFile;
	int err = concat_pidname(strFullPidFile,szPidFile);
	if (err)
		return err;

	// Block all SIGCHLD, and SIGHUP (as we disconnect from the controlling terminal)
	sigset_t sig,prev_sig;
	sigemptyset(&sig);
	sigaddset(&sig, SIGCHLD);
	sigaddset(&sig, SIGHUP);

	// Ignore all the terminal signals
	sigaddset(&sig, SIGTSTP);
	sigaddset(&sig, SIGTTOU);
	sigaddset(&sig, SIGTTIN);

	err = pthread_sigmask(SIG_BLOCK,&sig,&prev_sig);
	if (err)
		return err;

	// Now daemonize
	pid_t i = fork();
	if (i == (pid_t)-1)
		err = errno;
	else
	{
		// Parent exits
		if (i != (pid_t)0)
			_exit(0);

		// Create a new session
		if (setsid() == (pid_t)-1)
			err = errno;
		else
		{
			// Now fork again to prevent re-acquisition of a tty...
			i = fork();
			if (i == (pid_t)-1)
				err = errno;
			else
			{
				// Parent exits
				if (i != (pid_t)0)
					_exit(0);
			}
		}
	}

	// Restore previous signal mask
	int err2 = pthread_sigmask(SIG_SETMASK,&prev_sig,NULL);
	if (!err)
		err = err2;

	if (err)
		return err;

	// Open the pid file
	int fd;
	err = lock_pidfile(strFullPidFile,fd,already);
	if (err)
		return err;

	// Now close all open descriptors, except fd and stderr
	int except[] = { STDOUT_FILENO, STDERR_FILENO, fd };
	err = POSIX::close_file_descriptors(except,sizeof(except)/sizeof(except[0]));
	if (err)
		return errno;

	int n = POSIX::open("/dev/null",O_RDWR);
	if (n == -1)
		return errno;

	// Now close off stdin, stdout and stderr
	dup2(n,STDIN_FILENO);
	dup2(n,STDOUT_FILENO);
	dup2(n,STDERR_FILENO);
	POSIX::close(n);

	return 0;
}

#endif
