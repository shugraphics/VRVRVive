#include "Timer.h"

Timer::~Timer() {
}

void Timer::update(float d_t) {
	if ( m_running )
	{
		m_elapsedTime += d_t;
	}
}

void Timer::reset() {
	m_elapsedTime = 0.0;
}

void Timer::toggleRunning() {
	m_running = !m_running;
}

Timer::Timer(bool running) {
	m_elapsedTime = 0.0;
	m_running = running;
}

void Timer::clearSavedTimes() {
	m_savedTime.clear();
}

double Timer::getElapsedTime() const {
	return m_elapsedTime;
}

bool Timer::isRunning() const {
	return m_running;
}

void Timer::setRunning(bool running) {
	m_running = running;
}

double* Timer::getElapsedTimePtr() {
	return &m_elapsedTime;
}

bool* Timer::getRunningPtr() {
	return &m_running;
}

const std::vector<double>& Timer::getSavedTime() const {
	return m_savedTime;
}

void Timer::saveCurrentElapsedTime() {
	m_savedTime.push_back( m_elapsedTime );
}

GLFWTimer::GLFWTimer(bool running)
	: Timer( running )
{
	m_lastTime = - 1.0;
	if (running)
	{
		update(0.0f); // update immediately
	}
}

GLFWTimer::~GLFWTimer() {
}

#include <GL/glew.h>
#include <GLFW/glfw3.h>

void GLFWTimer::update(float d_t) {
	if ( m_running )
	{
		if ( m_lastTime != -1.0)
		{
			m_elapsedTime += glfwGetTime() - m_lastTime;
		}
		m_lastTime = glfwGetTime();
	}
}

void GLFWTimer::toggleRunning() {
	Timer::toggleRunning();

	(m_running) ? m_lastTime = glfwGetTime() : m_lastTime = -1.0;

}

void Timings::beginTimer(const std::string& timer)
{
	auto t = m_timers.find(timer);
	if ( t != m_timers.end())
	{
		(*t).second.start();
	}
	else
	{
		m_timers[timer] = new OpenGLTimer(); //create and start
	}
}

void Timings::stopTimer(const std::string& timer)
{
	auto t = m_timers.find(timer);
	if ( t != m_timers.end())
	{
		(*t).second.stop(); // save time 
	}
}
void Timings::resetTimer(const std::string& timer)
{
	auto t = m_timers.find(timer);
	if ( t != m_timers.end())
	{
		(*t).second.reset(); // save time 
	}
}
void Timings::deleteTimer(const std::string& timer)
{
	m_timers.erase(timer);
}

OpenGLTimer* Timings::getTimerPtr(const std::string& timer)
{
	auto t = m_timers.find(timer);
	if ( t != m_timers.end())
	{
		return &(*t).second;
	}
	return nullptr;
}

OpenGLTimer::OpenGLTimer(bool running)
	:
	m_executionTime(-1.0)
{
	m_queryID[0] = -1;
	m_queryID[1] = -1;
	if ( running )
	{
		this->start();
	}
}

OpenGLTimer::~OpenGLTimer()
{
	glDeleteQueries(2, &m_queryID[0]);
}


void OpenGLTimer::start()
{
	// generate OpenGL query objects
	glGenQueries(2, &m_queryID[0] );

	// request current time-stamp ( before dispatch )
	glQueryCounter(m_queryID[0], GL_TIMESTAMP);
}

void OpenGLTimer::stop()
{
	// request current time-stamp ( after dispatch )
	glQueryCounter(m_queryID[1], GL_TIMESTAMP);
}

void OpenGLTimer::reset()
{
	glDeleteQueries(2, &m_queryID[0]);
	m_queryID[0] = -1;
	m_queryID[1] = -1;
}

double OpenGLTimer::getTime()
{
	// wait for query to become available ( when dispatch finished )
	unsigned int stopTimerAvailable = 0;
	unsigned int timeoutCounter = 0;
	const unsigned int timoutLimit = 100000;
	if (glIsQuery(m_queryID[0]) && glIsQuery(m_queryID[1])) 
	{
		while (!stopTimerAvailable && timeoutCounter < timoutLimit)
		{
			glGetQueryObjectuiv(m_queryID[1],
	    			GL_QUERY_RESULT_AVAILABLE,
	    			&stopTimerAvailable);
			timeoutCounter++;
		}
		if ( timeoutCounter >= timoutLimit ) return m_executionTime;
	}
	// retrieve query results
	glGetQueryObjectui64v(m_queryID[0], GL_QUERY_RESULT, &m_startTime);
	glGetQueryObjectui64v(m_queryID[1], GL_QUERY_RESULT, &m_stopTime);

	// compute execution time
	m_executionTime = (m_stopTime - m_startTime) / 1000000.0;

	return m_executionTime;
}

#include <Core/DebugLog.h>
void OpenGLTimings::timestamp(const std::string& timestamp)
{
	if (!m_enabled) return;
	if (!glIsQuery(m_timestamps[timestamp].queryID)) 
	{
		glGenQueries(1, &(m_timestamps[timestamp].queryID) );
	}
	glQueryCounter(m_timestamps[timestamp].queryID, GL_TIMESTAMP);
}

void OpenGLTimings::beginTimerElapsed(const std::string& timer)
{
	if (!m_enabled) return;
	if (!glIsQuery(m_timersElapsed[timer].queryID[0]) || !glIsQuery(m_timersElapsed[timer].queryID[1])) 
	{
		glGenQueries(2, &(m_timersElapsed[timer].queryID[0]) );
	}
	glQueryCounter(m_timersElapsed[timer].queryID[0], GL_TIMESTAMP);
	glBeginQuery(GL_TIME_ELAPSED, m_timersElapsed[timer].queryID[1]);
}
void OpenGLTimings::stopTimerElapsed()
{
	if (!m_enabled) return;
	glEndQuery(GL_TIME_ELAPSED);
}

void OpenGLTimings::beginTimer(const std::string& timer)
{
	if (!m_enabled) return;
	if (!glIsQuery(m_timers[timer].queryID[0]) || !glIsQuery(m_timers[timer].queryID[1])) 
	{
		glGenQueries(2, &(m_timers[timer].queryID[0]) );
	}
	glQueryCounter(m_timers[timer].queryID[0], GL_TIMESTAMP);
}
void OpenGLTimings::stopTimer(const std::string& timer)
{
	if (!m_enabled) return;
	glQueryCounter(m_timers[timer].queryID[1], GL_TIMESTAMP);
}
void OpenGLTimings::updateReadyTimings()
{
	if (!m_enabled) return;
	
	// timestamps
	for ( auto kv = m_timestamps.begin(); kv != m_timestamps.end(); ++kv)
	{
		glGetQueryObjectui64v((*kv).second.queryID, GL_QUERY_RESULT_NO_WAIT, &(*kv).second.timestamp);
		(*kv).second.lastTime = (*kv).second.timestamp / 1000000.0;
	}

	// timestamps + time elapsed
	for ( auto kv = m_timersElapsed.begin(); kv != m_timersElapsed.end(); ++kv)
	{
		glGetQueryObjectui64v((*kv).second.queryID[0], GL_QUERY_RESULT_NO_WAIT, &(*kv).second.startTime);
		glGetQueryObjectui64v((*kv).second.queryID[1], GL_QUERY_RESULT_NO_WAIT, &(*kv).second.elapsedTime);
		(*kv).second.lastTime = (*kv).second.startTime / 1000000.0;
		(*kv).second.lastTiming = (*kv).second.elapsedTime/ 1000000.0;
	}

	// time elapsed (using timestamps)
	for ( auto kv = m_timers.begin(); kv != m_timers.end(); ++kv)
	{
		glGetQueryObjectui64v((*kv).second.queryID[0], GL_QUERY_RESULT_NO_WAIT, &(*kv).second.startTime);
		glGetQueryObjectui64v((*kv).second.queryID[1], GL_QUERY_RESULT_NO_WAIT, &(*kv).second.stopTime);
		(*kv).second.lastTime = (*kv).second.startTime / 1000000.0;
		(*kv).second.lastTiming = ((*kv).second.stopTime - (*kv).second.startTime) / 1000000.0;
	}
}

OpenGLTimings::Timer OpenGLTimings::waitForTimerResult(const std::string& timer)
{
	OpenGLTimings::Timer result;
	auto kv = m_timers.find(timer);
	if (kv != m_timers.end())
	{
		// wait until the results are available
		GLint stopTimerAvailable = 0;
		while (!stopTimerAvailable) {
			glGetQueryObjectiv((*kv).second.queryID[1], GL_QUERY_RESULT_AVAILABLE, &stopTimerAvailable);
		}

		glGetQueryObjectui64v((*kv).second.queryID[0], GL_QUERY_RESULT, &(*kv).second.startTime);
		glGetQueryObjectui64v((*kv).second.queryID[1], GL_QUERY_RESULT, &(*kv).second.stopTime);
		(*kv).second.lastTime = (*kv).second.startTime / 1000000.0;
		(*kv).second.lastTiming = ((*kv).second.stopTime - (*kv).second.startTime) / 1000000.0;
		result = (*kv).second;
	}

	return result;
}

OpenGLTimings::Timestamp OpenGLTimings::waitForTimestampResult(const std::string& timestamp)
{
	OpenGLTimings::Timestamp result;
	auto kv = m_timestamps.find(timestamp);
	if (kv != m_timestamps.end())
	{
		// wait until the results are available
		GLint timestampAvailable = 0;
		while (!timestampAvailable) {
			glGetQueryObjectiv((*kv).second.queryID, GL_QUERY_RESULT_AVAILABLE, &timestampAvailable);
		}

		glGetQueryObjectui64v((*kv).second.queryID, GL_QUERY_RESULT, &(*kv).second.timestamp);
		(*kv).second.lastTime = (*kv).second.timestamp / 1000000.0;
		result = (*kv).second;
	}

	return result;
}