from Plugins.Plugin import PluginDescriptor
from enigma import eDVBResourceManager
from Components.NimManager import nimmanager

from threading import Thread, Semaphore, Lock

import pydevd
import DreamtunerAPI
import time
    
class Request(Thread):
    def __init__(self):
        Thread.__init__(self)
        self.res_mgr = None
        # tuner_group per frontend as tuple
        # only allowed frontends are listed
        self.frontends = [(0,2),(1,3),(2,252),(3,65533)]  
        
    def run(self):
        DreamtunerAPI.DEBUG("Request.run start")
        pydevd.settrace( host="192.168.1.100", stdoutToServer=True, stderrToServer=True )
        while True:
            req = DreamtunerAPI.fetch_request()
            DreamtunerAPI.INFO("Request received ip:{0} tuner_type:{1} tuner_group:{2}".format(req[0],req[1],req[2]))
            
            Channel = None
            feid = -1
            for (feid,tuner_group) in self.frontends:
                # check if the tuner_group matches 
                if (tuner_group & req[2]) != 0:
                    Channel = self.allocateChannel(req[1], feid)
                    if Channel:
                        break
                else:
                    DreamtunerAPI.DEBUG("Frontend {0}: tuner group mismatch {1} and {2}".format(feid, tuner_group, req[2]))
            
            if Channel: 
                DreamtunerAPI.INFO("start worker for {0} with frontend {1}".format(req[0],feid))
                worker = Worker(req[0], Channel, feid)
                worker.start()
            else:
                DreamtunerAPI.INFO("no idle frontend available for request from {0}".format(req[0]))
            
    def allocateChannel(self, tuner_type, feid):
        fe_types = ((""),                             # 0
                    ("DVB-S"),                        # 1
                    ("DVB-C"),                        # 2
                    ("DVB-S", "DVB-C"),               # 3
                    ("DVB-T"),                        # 4
                    ("DVB-S", "DVB-T"),               # 5  
                    ("DVB-C", "DVB-T"),               # 6 
                    ("DVB-S", "DVB-C", "DVB-T"),      # 7 
                    ("DVB-S2"),                             # 8
                    ("DVB-S2", "DVB-S"),                    # 9
                    ("DVB-S2", "DVB-C"),                    # 10
                    ("DVB-S2", "DVB-S", "DVB-C"),           # 11
                    ("DVB-S2", "DVB-T"),                    # 12
                    ("DVB-S2", "DVB-S", "DVB-T"),           # 13 
                    ("DVB-S2", "DVB-C", "DVB-T"),           # 14
                    ("DVB-S2", "DVB-S", "DVB-C", "DVB-T"))  # 15                   )
        
        if not self.res_mgr:
            self.res_mgr = eDVBResourceManager.getInstance()
    
        if self.res_mgr: 
            Channel = self.res_mgr.allocateRawChannel(feid)
            if Channel:
                DreamtunerAPI.DEBUG("allocated channel on frontend {0}".format(feid))
                fe_type = nimmanager.getNimType(feid)
                # compare frontend types
                if fe_type in fe_types[tuner_type]:
                    DreamtunerAPI.INFO("frontend type matches the request")
                else:
                    DreamtunerAPI.INFO("frontend type didn't match the request: {0} {1}".format(fe_type, fe_types[tuner_type]))
                    Channel = None
            else:
                DreamtunerAPI.DEBUG("can't allocate channel on frontend {0}".format(feid))
                    
        return Channel
    
class Worker(Thread):
    def __init__(self, IP, Channel, feid):
        Thread.__init__(self)
        self.IP = IP
        self.Channel = Channel
        self.feid = feid
        
    def run(self):
        demuxid = self.Channel.reserveDemux()
        # run the worker that will handle the request
        # this call will not return until the client 
        # is disconnected
        DreamtunerAPI.run_worker(self.IP, self.feid, demuxid)
        
        # release the allocated frontend and demux
        self.Channel = None

RequestThread = None

def autostart(reason, **kwargs):
#    pydevd.settrace( host="192.168.1.100", stdoutToServer=True, stderrToServer=True )
    DreamtunerAPI.set_usesyslog( True )
    DreamtunerAPI.INFO("autostart end")

def networkstart(reason, **kwargs):
    DreamtunerAPI.DEBUG("networkstart start")
#    pydevd.settrace( host="192.168.1.100", stdoutToServer=True, stderrToServer=True )
    RequestThread = Request()
    RequestThread.start()
    DreamtunerAPI.INFO("networkstart end")

def openconfig(session, **kwargs):
    DreamtunerAPI.DEBUG("openconfig start")
    pydevd.settrace( host="192.168.1.100", stdoutToServer=True, stderrToServer=True )
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