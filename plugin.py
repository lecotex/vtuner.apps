from Plugins.Plugin import PluginDescriptor
from enigma import eDVBResourceManager, eTimer
from Components.NimManager import nimmanager

from threading import Thread, Condition, Lock

import pydevd
import DreamtunerAPI
import time

request_pipeline = None
res_mgr = None

class Request():
    def __init__(self, (ip, tuner_type, tuner_group)):
        self.ip = ip
        self.tuner_type = tuner_type
        self.tuner_group = tuner_group
        self.Channel = None 
        self.feid = -1
        self.demux = -1

    def allocateResourceses(self, feid):
        global res_mgr
        if not res_mgr:
            res_mgr = eDVBResourceManager.getInstance()
    
        if res_mgr: 
            self.Channel = res_mgr.allocateRawChannel(feid)
            if self.Channel:
                DreamtunerAPI.DEBUG("allocated channel on frontend {0}".format(feid))
            else:
                DreamtunerAPI.DEBUG("can't allocate channel on frontend {0}".format(feid))
        
        if self.Channel:
            self.feid = feid
            self.demuxid = self.Channel.reserveDemux()
            if self.demuxid < 0:
                self.Channel = None
                DreamtunerAPI.DEBUG("can't allocate demux on frontend {0}".format(feid))
                
        return self.Channel != None 
            
class RequestPipeline():
    def __init__(self):
        self.lock = Lock()  # one giant lock, to keep things simple
        self.request_queue = []
        self.release_queue = []
        self.frontends = []
        
    def findFrontend(self, feid):
        ind = len(self.frontends) - 1
        while(ind != -1):
            if self.frontends[ind][0] == feid:
                break
            ind -= 1
        return ind
        
    def setFrontend(self, feid, tuner_group):
        ind = self.findFrontend(feid)
        self.lock.acquire()
        if ind == -1:
            try:
                nimtype = nimmanager.getNimType(feid)
                tuner_type_dict = {'DVB-S':1, 'DVB-C':2, 'DVB-T':4, 'DVB-S2':8 }
                self.frontends.append([feid,tuner_type_dict[nimtype],tuner_group])
            except:
                DreamtunerAPI.WARN("frontend {0} is not available.".format(feid))
        else:
            self.frontends[ind][2] = tuner_group
        self.lock.release()

    def removeFrontend(self, feid):
        ind = self.findFrontend(feid)
        if ind != -1:
            self.lock.acquire()
            self.frontends.pop(ind)
            self.lock.release()
    
    # this method has to be called to feed a request
    # into the pipeline
    # callable from any thread context
    def pushRequest(self, request):
        self.lock.acquire()
#        DreamtunerAPI.DEBUG("pushRequest - got lock")
        self.request_queue.append(request)
#        DreamtunerAPI.DEBUG("pushRequest - free lock")
        self.lock.release()
        return  
    
    # check if a matching and idle frontend is available
    # if so allocate resources and start the worker thread
    # this must be called frequently from the main
    # enigma thread, eg. timer
    def handleRequest(self):
        self.lock.acquire()
#        DreamtunerAPI.DEBUG("handleRequest - got lock")
        while len(self.request_queue) > 0:
            request = self.request_queue.pop(0)
            for (feid, tuner_types, tuner_group) in self.frontends:
                # check if tuner_group and type are matching matches 
                if (tuner_group & request.tuner_group) != 0 and (tuner_types & request.tuner_type) != 0:
                    if request.allocateResourceses(feid):
                        DreamtunerAPI.INFO("start worker for {0} with frontend {1}".format(request.ip, request.feid))
                        worker = Worker(request)
                        worker.start()
                        break
                else:
                    DreamtunerAPI.DEBUG("Frontend {0}: tuner group mismatch {1} and {2}".format(feid, tuner_group, request.tuner_group))
            DreamtunerAPI.INFO("No frontend found for request.")
#        DreamtunerAPI.DEBUG("handleRequest - free lock")
        self.lock.release()
            
    # will be called from the worker after termination, to
    # schedule resource deallocation
    # callable from any thread context         
    def pushRelease(self, request):
        self.lock.acquire()
        DreamtunerAPI.DEBUG("pushRelease - got lock")
        self.release_queue.append(request)
        return self.lock
    
    # free the allocated resources
    # this must be called frequently from the main
    # enigma thread, eg. timer
    def handleRelease(self):
        self.lock.acquire()
#        DreamtunerAPI.DEBUG("handleRelease - got lock")
        while len(self.release_queue) > 0:
            request = self.release_queue.pop(0)
            DreamtunerAPI.INFO("Release frontend:{0} demux:{1}".format(request.feid,request.demuxid))
            
#        DreamtunerAPI.DEBUG("handleRelease - free lock")
        self.lock.release()
                            
class RequestThread(Thread):
    def __init__(self):
        Thread.__init__(self)
        self.daemon = True
        
    def run(self):
        DreamtunerAPI.DEBUG("Request.run start")
#        pydevd.settrace( host="192.168.1.100", stdoutToServer=True, stderrToServer=True )
        while True:
            global request_pipeline
            request = Request(DreamtunerAPI.fetch_request())
            DreamtunerAPI.INFO("Request received ip:{0} tuner_type:{1} tuner_group:{2}".format(request.ip, request.tuner_type, request.tuner_group))
            request_pipeline.pushRequest(request)
                
class Worker(Thread):
    def __init__(self, request):
        Thread.__init__(self)
        self.daemon = True
        self.request = request
        
    def run(self):
#        pydevd.settrace( host="192.168.1.100", stdoutToServer=True, stderrToServer=True )
        
        global request_pipeline
        # run the worker that will handle the request
        # this call will not return until the client 
        # is disconnected
        DreamtunerAPI.run_worker(self.request.ip, self.request.feid, self.request.demuxid)
        
        # release the allocated frontend and demux
        # we can not release the the reference in the context 
        # of this thread, as enigma isn't thread safe.
        # instead we pass a reference to a queue for later
        # destruction by the main thread. To prevent a race 
        # condition a lock is held until the reference is destroyed 
        # in this thread. 
        lock = request_pipeline.pushRelease(self.request)
        self.request = None
        lock.release()
        DreamtunerAPI.DEBUG("worker run - free lock")

timer = eTimer()

def timerLoop():
    global request_pipeline
    DreamtunerAPI.INFO("timerLoop")
    request_pipeline.handleRelease()
    request_pipeline.handleRequest()        
        
def autostart(reason, **kwargs):
#    pydevd.settrace( host="192.168.1.100", stdoutToServer=True, stderrToServer=True )
    DreamtunerAPI.set_usesyslog( True )
    DreamtunerAPI.INFO("autostart end")

def networkstart(reason, **kwargs):
    DreamtunerAPI.DEBUG("networkstart start")
    pydevd.settrace( host="192.168.1.100", stdoutToServer=True, stderrToServer=True )
    
    global request_pipeline
    request_pipeline = RequestPipeline()

    global timer    
    timer.callback.append(timerLoop)
    timer.start(500)
    
    rt = RequestThread()
    rt.start()
    
    DreamtunerAPI.INFO("networkstart end")

def openconfig(session, **kwargs):
    DreamtunerAPI.DEBUG("openconfig start")
    pydevd.settrace( host="192.168.1.100", stdoutToServer=True, stderrToServer=True )

    # setFrontend/removeFrontend will be executed by the config dialog
    request_pipeline.setFrontend(0, 2)
    request_pipeline.setFrontend(1, 3)
    request_pipeline.setFrontend(2, 252)
    request_pipeline.setFrontend(3, 65533)
    # test frontend overwrite
    request_pipeline.setFrontend(1, 0)
    # test frontend remove
    request_pipeline.removeFrontend(1)
        
    DreamtunerAPI.INFO("openconfig end")

def Plugins(**kwargs):
    return [ 
        PluginDescriptor(
            name="DreamTuner",
            description="DreamTuner resource management and discover",
            where = PluginDescriptor.WHERE_AUTOSTART,
            fnc=autostart
        ),
        PluginDescriptor(
            where=PluginDescriptor.WHERE_NETWORKCONFIG_READ, 
            fnc=networkstart
        ),
        PluginDescriptor(
            name="DreamTuner", 
            description="DreamTuner Configuration",
            where = PluginDescriptor.WHERE_PLUGINMENU,
            fnc=openconfig 
        )
    ]
