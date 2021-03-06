from Screens.Screen import Screen
from Screens.MessageBox import MessageBox
from Components.ConfigList import ConfigListScreen
from Components.config import config, getConfigListEntry, ConfigSubsection, ConfigInteger, ConfigYesNo, ConfigText, ConfigSelection, ConfigSubList, ConfigIP
from Components.Sources.StaticText import StaticText
from Components.MenuList import MenuList
from Components.MultiContent import MultiContentEntryText
from Components.ActionMap import ActionMap

config.plugins.vTuner = ConfigSubsection()
config.plugins.vTuner.client = ConfigSubsection()
config.plugins.vTuner.client.enabled = ConfigYesNo(default=True)
config.plugins.vTuner.client.mode = ConfigSubList()
config.plugins.vTuner.client.discover = ConfigYesNo(default=True)
config.plugins.vTuner.client.ip = ConfigIP( default=[0,0,0,0] )
config.plugins.vTuner.client.port = ConfigInteger(default = 39305, limits=(0, 49152) )
   
for i in [0,1,2]:
    mode = ConfigSubsection()
    if i == 0:
        # first type must not be none
        mode.type = ConfigSelection(default="s2", choices = [("s2", _("DVB-S2 connected to DVB-S/S2")),("c", _("DVB-C")),("t", _("DVB-T")), ("S2", _("DVB-S/S2 connected to DVB-S2")), ("s", _("DVB-S connected to DVB-S/S2")), ("S", _("DVB-S connected to DVB-S")) ])
    else:
        mode.type = ConfigSelection(default="", choices = [("", _("unused")), ("s2", _("DVB-S2 connected to DVB-S/S2")),("c", _("DVB-C")),("t", _("DVB-T")), ("S2", _("DVB-S2 connected to DVB-S2")), ("s", _("DVB-S connected to DVB-S/S2")), ("S", _("DVB-S connected to DVB-S")) ])
    mode.group = ConfigInteger(default = 1, limits=(1, 255) )
    config.plugins.vTuner.client.mode.append(mode)
    
config.plugins.vTuner.server = ConfigSubsection()
config.plugins.vTuner.server.enabled = ConfigYesNo(default=False) 

config.plugins.vTuner.expert = ConfigSubsection()
config.plugins.vTuner.expert.enabled = ConfigYesNo(default=False)
config.plugins.vTuner.expert.buffersize = ConfigInteger(default = 696, limits=(200, 2000) )
config.plugins.vTuner.expert.readdelay = ConfigInteger(default = 50, limits=(0, 300) )
    
def configCB(result, session):
    if result is True:
        f = open('/etc/vtunerc.conf','w')
        f.write("# this file is auto generated and will be overwritten\n")
        f.write("# DO NOT EDIT\n")
        if config.plugins.vTuner.client.enabled.value:
            f.write('DAEMON="/usr/sbin/vtunerc.mipsel"\n')
        else:
            f.write('DAEMON="/bin/true"\n')
        
        options=""
        if not config.plugins.vTuner.client.discover.value:
            options += "-n %d.%d.%d.%d" % tuple(config.plugins.vTuner.client.ip.value)
            options += ":{0} ".format(config.plugins.vTuner.client.port.value)
            
        if config.plugins.vTuner.expert.enabled.value:
            options += "-r %d " % config.plugins.vTuner.expert.buffersize.value
            options += "-x %d " % config.plugins.vTuner.expert.readdelay.value
        
        first_mode = True
        for mode in config.plugins.vTuner.client.mode:
            if mode.type.value:
                if first_mode:
                    options += "-f {0}:{1}".format(mode.type.value, mode.group.value)
                else:
                    options += ",{0}:{1}".format(mode.type.value, mode.group.value)
                first_mode = False            
                
        f.write('OPTIONS="{0}"\n'.format(options))

class vTunerConfigScreen(ConfigListScreen, Screen):        
    skin = """
        <screen name="vTunerConfigScreen" position="center,center" size="560,400" title="Dream Tuner: Main Setup">
             <ePixmap pixmap="skin_default/buttons/red.png" position="0,0" size="140,40" alphatest="on" />
             <ePixmap pixmap="skin_default/buttons/green.png" position="140,0" size="140,40" alphatest="on" />
             <widget source="key_red" render="Label" position="0,0" zPosition="1" size="140,40" font="Regular;20" halign="center" valign="center" backgroundColor="#9f1313" transparent="1"/>
             <widget source="key_green" render="Label" position="140,0" zPosition="1" size="140,40" font="Regular;20" halign="center" valign="center" backgroundColor="#1f771f" transparent="1"/>
             <widget name="config" position="5,50" size="550,360" scrollbarMode="showOnDemand" zPosition="1"/>
        </screen>"""

    def __init__(self, session, args=0):
        Screen.__init__(self, session)
        ConfigListScreen.__init__(self, [])
                
        self.createSetup()

        self["key_red"] = StaticText(_("Cancel"))
        self["key_green"] = StaticText(_("OK"))
        self["key_yellow"] = StaticText("")
        self["setupActions"] = ActionMap(
             ["SetupActions", "ColorActions"],
             {"red": self.cancel,
              "green": self.save,
              "save": self.save,
              "cancel": self.cancel,
              "ok": self.save,
             }, -2)

        self.onLayoutFinish.append(self.layoutFinished)

    def keyLeft(self):
        ConfigListScreen.keyLeft(self)
        self.createSetup()

    def keyRight(self):
        ConfigListScreen.keyRight(self)
        self.createSetup()

    def createSetupClient(self):
        list = [ ]
        
        list.append( getConfigListEntry( _("Autodiscover server:"), config.plugins.vTuner.client.discover) )
        if not config.plugins.vTuner.client.discover.value:
            list.append( getConfigListEntry( _("Server IP:"), config.plugins.vTuner.client.ip) )
            list.append( getConfigListEntry( _("Server port:"), config.plugins.vTuner.client.port) )
            
        for n in [0,1,2]:
            mode = config.plugins.vTuner.client.mode[n]
            list.append( getConfigListEntry( _("{0}. Tuner type:".format(n+1)), mode.type) )
            if mode.type.value != "":
                list.append( getConfigListEntry( _("Tuner group (unsupported):"), mode.group) )
        
        list.append( getConfigListEntry( _("Expert settings:"), config.plugins.vTuner.expert.enabled) )
        if config.plugins.vTuner.expert.enabled.value:
            list.append( getConfigListEntry( _("Client buffer size:"), config.plugins.vTuner.expert.buffersize) )
            list.append( getConfigListEntry( _("Client read delay [ms]:"), config.plugins.vTuner.expert.readdelay) )
            
        return(list)
        
    def createSetup(self):
        list = [ ]
        
        list.append( getConfigListEntry(_("Start Client"), config.plugins.vTuner.client.enabled) )
        if config.plugins.vTuner.client.enabled.value:
            list.extend(self.createSetupClient())
        
        self["config"].list = list
        self["config"].l.setList(list)
            
    def layoutFinished(self):
        self.setTitle(_("DreamTuner: Main Setup"))

    def save(self):
        for x in self["config"].list:
            x[1].save()
        self.close(True, self.session)

    def cancel(self):
        for x in self["config"].list:
            x[1].cancel()
        self.close(False, self.session)


