
#ifndef LOGGER_H
#define LOGGER_H

#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <mutex>
#include <time.h>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define SOCKET int
#define INVALID_SOCKET -1

namespace simplelogger{

enum LogLevel {
	TRACE,
	DEBUG,
	INFO,
	WARN,
	ERR
};

class Logger {
public:
	Logger(LogLevel level, bool bPrintTimeStamp) : level(level), bPrintTimeStamp(bPrintTimeStamp) {}
	virtual ~Logger() {}
	virtual std::ostream& GetStream() = 0;
	virtual void FlushStream() {}
	bool ShouldLogFor(LogLevel l) {
		return l >= level;
	}
	char* GetLead(LogLevel l, const char *szFile, int nLine, const char *szFunc) {
		if (l < TRACE || l > ERR) {
		    sprintf(szLead, "[?????] ");
			return szLead;
		}
		const char *szLevels[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR"};
		if (bPrintTimeStamp) {
			time_t t = time(NULL);
			struct tm *ptm = localtime(&t);
			sprintf(szLead, "[%-5s][%02d:%02d:%02d] ",
				szLevels[l], ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
		} else {
			sprintf(szLead, "[%-5s] ", szLevels[l]);
		}
		return szLead;
	}
	void EnterCriticalSection() {
		mtx.lock();
	}
	void LeaveCriticalSection() {
		mtx.unlock();
	}
private:
	LogLevel level;
	char szLead[80];
	bool bPrintTimeStamp;
	std::mutex mtx;
};

class LoggerFactory {
public:
	static Logger* CreateFileLogger(std::string strFilePath,
			LogLevel level = DEBUG, bool bPrintTimeStamp = true) {
		return new FileLogger(strFilePath, level, bPrintTimeStamp);
	}
	static Logger* CreateConsoleLogger(LogLevel level = DEBUG,
			bool bPrintTimeStamp = true) {
		return new ConsoleLogger(level, bPrintTimeStamp);
	}
	static Logger* CreateUdpLogger(char *szHost, unsigned uPort, LogLevel level = DEBUG,
			bool bPrintTimeStamp = true) {
		return new UdpLogger(szHost, uPort, level, bPrintTimeStamp);
	}
private:
	LoggerFactory() {}

	class FileLogger : public Logger {
	public:
		FileLogger(std::string strFilePath, LogLevel level, bool bPrintTimeStamp)
		: Logger(level, bPrintTimeStamp) {
			pFileOut = new std::ofstream();
			pFileOut->open(strFilePath.c_str());
		}
		~FileLogger() {
			pFileOut->close();
		}
		std::ostream& GetStream() {
			return *pFileOut;
		}
	private:
		std::ofstream *pFileOut;
	};

	class ConsoleLogger : public Logger {
	public:
		ConsoleLogger(LogLevel level, bool bPrintTimeStamp)
		: Logger(level, bPrintTimeStamp) {}
		std::ostream& GetStream() {
			return std::cout;
		}
	};

	class UdpLogger : public Logger {
	private:
		class UdpOstream : public std::ostream {
		public:
			UdpOstream(char *szHost, unsigned short uPort) : std::ostream(&sb), socket(INVALID_SOCKET){
				socket = ::socket(AF_INET, SOCK_DGRAM, 0);
				if (socket == INVALID_SOCKET) {
					fprintf(stderr, "socket() failed.\n");
					return;
				}
                struct in_addr addr = {inet_addr(szHost)};
				struct sockaddr_in s = {AF_INET, htons(uPort), addr};
				server = s;
			}
			virtual ~UdpOstream() {
				if (socket == INVALID_SOCKET) {
					return;
				}
                close(socket);
			}
			void Flush() {
				if (sendto(socket, sb.str().c_str(), (int)sb.str().length() + 1,
						0, (struct sockaddr *)&server, (int)sizeof(sockaddr_in)) == -1) {
					fprintf(stderr, "sendto() failed.\n");
				}
				sb.str("");
			}

		private:
			std::stringbuf sb;
			SOCKET socket;
			struct sockaddr_in server;
		};
	public:
		UdpLogger(char *szHost, unsigned uPort, LogLevel level, bool bPrintTimeStamp)
		: Logger(level, bPrintTimeStamp), udpOut(szHost, (unsigned short)uPort) {}
		UdpOstream& GetStream() {
			return udpOut;
		}
		virtual void FlushStream() {
			udpOut.Flush();
		}
	private:
		UdpOstream udpOut;
	};
};

}

#define LOG_(pLogger, event, level) \
	do {													\
		if (!pLogger || !pLogger->ShouldLogFor(level)) {	\
			break;											\
		}													\
		pLogger->EnterCriticalSection();					\
		pLogger->GetStream()								\
			<< pLogger->GetLead(level, __FILE__, __LINE__,	\
				__FUNCTION__)								\
			<< event << std::endl;							\
		pLogger->FlushStream();								\
		pLogger->LeaveCriticalSection();					\
	} while (0);

#define LOG_TRACE(pLogger, event)	LOG_(pLogger, event, simplelogger::TRACE)
#define LOG_DEBUG(pLogger, event)	LOG_(pLogger, event, simplelogger::DEBUG)
#define LOG_INFO(pLogger, event)	LOG_(pLogger, event, simplelogger::INFO)
#define LOG_WARN(pLogger, event)	LOG_(pLogger, event, simplelogger::WARN)
#define LOG_ERROR(pLogger, event)	LOG_(pLogger, event, simplelogger::ERR)


#endif // LOGGER_H
