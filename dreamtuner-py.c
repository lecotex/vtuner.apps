#include <Python.h>
#include <string.h>
#include <syslog.h>
#include <stdarg.h>

#define VERSION "0815"

#define ERROR(msg, ...) write_message(0x01, VERSION " [%s:%u] error: " msg, __FILE__, __LINE__, ## __VA_ARGS__)
#define  WARN(msg, ...) write_message(0x02, VERSION " [%s:%u] warn: " msg,  __FILE__, __LINE__, ## __VA_ARGS__)
#define  INFO(msg, ...) write_message(0x04, VERSION " [%s:%u] info: " msg,  __FILE__, __LINE__, ## __VA_ARGS__)
#define DEBUG(msg, ...) write_message(0x0010, VERSION " [%s:%u] debug: " msg, __FILE__, __LINE__, ## __VA_ARGS__)

#define MAX_MSGSIZE 1024

int dbg_level  = 0xff;
int use_syslog = 1;

__thread char msg[MAX_MSGSIZE];

void write_message(int level, const char* fmt, ... ) {
	if( level & dbg_level  ) {

		char tn[MAX_MSGSIZE];
		va_list ap;
		va_start(ap, fmt);
		vsnprintf(tn, sizeof(tn), fmt, ap);
		va_end(ap);
		strncat(msg, tn, sizeof(msg));

		if(use_syslog) {
			int priority;
			switch(level) {
				case 0: priority=LOG_ERR; break;
				case 1: priority=LOG_WARNING; break;
				case 2: priority=LOG_INFO; break;
				default: priority=LOG_DEBUG; break;
			}
			syslog(priority, "%s", msg);
		} else {
			fprintf(stderr, "%s", msg);
		}
	}
	strncpy(msg, "", sizeof(msg));
}

static PyObject * log_message(int severity, PyObject *args) {
	char *s;
	if (!PyArg_ParseTuple(args, "s", &s)) return NULL;
	write_message(severity, s);
	return Py_BuildValue("i", 0);
}

static PyObject * _py_ERROR(PyObject *self, PyObject *args) {
	return log_message(0x01, args);
}

static PyObject * _py_WARN(PyObject *self, PyObject *args) {
	return log_message(0x02, args);
}

static PyObject * _py_INFO(PyObject *self, PyObject *args) {
	return log_message(0x04, args);
}

static PyObject * _py_DEBUG(PyObject *self, PyObject *args) {
	return log_message(0x10, args);
}

static PyObject * _py_set_debuglevel(PyObject *self, PyObject *args) {
	char *sdbg;
	if (!PyArg_ParseTuple(args, "s", &sdbg)) return NULL;
	dbg_level=atoi(sdbg);
	DEBUG("_py_set_debuglevel %x\n", dbg_level);
	return Py_BuildValue("i", 0);
}

static PyObject * _py_set_usesyslog(PyObject *self, PyObject *args) {
	if (!PyArg_ParseTuple(args, "i", &use_syslog)) return NULL;
	DEBUG("_py_set_usesyslog %d\n", use_syslog);
	return Py_BuildValue("i", 0);
}

static PyObject * _py_fetch_request(PyObject *self, PyObject *args) {
	char *ip = "333.333.333.333";
	int tuner_group = 2;
	int tuner_type = 9;

	DEBUG("_py_fetch_request %d\n");
	Py_BEGIN_ALLOW_THREADS;
	/*
	 * The code to listen to the client request goes here. As we
	 * drop the Python GIL, it's ok to block until the next
	 * request arrives (which maybe forever).
	 * We can not answer the request at this point as we don't
	 * know if a appropriate tuner is available and unused.
	 */
	sleep(1);
	Py_END_ALLOW_THREADS;
	return Py_BuildValue("(sii)", ip, tuner_type, tuner_group);
}

static PyObject * _py_run_worker(PyObject *self, PyObject *args) {
	char *ip;
	int *fe;
	int *demux;

	DEBUG("_py_run_worker %d\n");
	if (!PyArg_ParseTuple(args, "sii", &ip, &fe, &demux)) return NULL;
	Py_BEGIN_ALLOW_THREADS;
	DEBUG("Offering fronted:%d demux:%d to %s", fe, demux, ip);
	/*
	 * The code to handle the request goes here. we have to prepare
	 * TCP sockets for the data and control connection and then
	 * answer the request from the client with the ports in use.
	 * We have to be careful as the client may have chosen a offer
	 * from another server. I'd guess that 10s should be enough to wait
	 * for the initial connect from the client.
	 */
	sleep(5);
	Py_END_ALLOW_THREADS;
	return Py_BuildValue("i", 0);
}

static PyMethodDef DreamtunerAPIMethodes[] = {
	{"ERROR",          _py_ERROR,          METH_VARARGS, "provide common logging facility for python code"},
	{"WARN",           _py_WARN,           METH_VARARGS, "provide common logging facility for python code"},
	{"INFO",           _py_INFO,           METH_VARARGS, "provide common logging facility for python code"},
	{"DEBUG",          _py_DEBUG,          METH_VARARGS, "provide common logging facility for python code"},
	{"set_debuglevel", _py_set_debuglevel, METH_VARARGS, "set level for logged messages"},
	{"set_usesyslog",  _py_set_usesyslog,  METH_VARARGS, "use syslog to log messages"},
	{"fetch_request",  _py_fetch_request,  METH_VARARGS, "wait and return a request for a tuner"},
	{"run_worker",     _py_run_worker,     METH_VARARGS, "serves a tuner request"},
	{NULL, NULL, 0, NULL}
};

PyMODINIT_FUNC initDreamtunerAPI(void) {
	Py_InitModule("DreamtunerAPI", DreamtunerAPIMethodes);
	openlog("dreamtuner", LOG_PERROR, LOG_USER);
}
