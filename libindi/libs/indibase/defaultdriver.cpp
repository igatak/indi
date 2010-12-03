#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <zlib.h>

#include "defaultdriver.h"
#include "indicom.h"
#include "base64.h"


INDI::DefaultDriver::DefaultDriver()
{
    pDebug = false;
    pSimulation = false;
}

bool INDI::DefaultDriver::loadConfig()
{
    char errmsg[MAXRBUF];
    bool pResult = false;

    pResult = IUReadConfig(NULL, deviceID, errmsg) == 0 ? true : false;

   if (pResult)
       IDMessage(deviceID, "Configuration successfully loaded.");

   IUSaveDefaultConfig(NULL, NULL, deviceID);

   return pResult;
}

bool INDI::DefaultDriver::saveConfig()
{
    std::vector<pOrder>::const_iterator orderi;
    ISwitchVectorProperty *svp=NULL;
    char errmsg[MAXRBUF];
    FILE *fp = NULL;

    fp = IUGetConfigFP(NULL, deviceID, errmsg);

    if (fp == NULL)
    {
        IDMessage(deviceID, "Error saving configuration. %s", errmsg);
        return false;
    }

    IUSaveConfigTag(fp, 0);

    for (orderi = pAll.begin(); orderi != pAll.end(); orderi++)
    {
        switch ( (*orderi).type)
        {
        case INDI_NUMBER:
             IUSaveConfigNumber(fp, static_cast<INumberVectorProperty *> ((*orderi).p));
             break;
        case INDI_TEXT:
             IUSaveConfigText(fp, static_cast<ITextVectorProperty *> ((*orderi).p));
             break;
        case INDI_SWITCH:
             svp = static_cast<ISwitchVectorProperty *> ((*orderi).p);
             /* Never save CONNECTION property. Don't save switches with no switches on if the rule is one of many */
             if (!strcmp(svp->name, "CONNECTION") || (svp->r == ISR_1OFMANY && !IUFindOnSwitch(svp)))
                 continue;
             IUSaveConfigSwitch(fp, svp);
             break;
        case INDI_BLOB:
             IUSaveConfigBLOB(fp, static_cast<IBLOBVectorProperty *> ((*orderi).p));
             break;
        }
    }

    IUSaveConfigTag(fp, 1);

    fclose(fp);

    IUSaveDefaultConfig(NULL, NULL, deviceID);

    IDMessage(deviceID, "Configuration successfully saved.");

    return true;
}

bool INDI::DefaultDriver::loadDefaultConfig()
{
    char configDefaultFileName[MAXRBUF];
    char errmsg[MAXRBUF];
    bool pResult = false;

    if (getenv("INDICONFIG"))
        snprintf(configDefaultFileName, MAXRBUF, "%s.default", getenv("INDICONFIG"));
    else
        snprintf(configDefaultFileName, MAXRBUF, "%s/.indi/%s_config.xml.default", getenv("HOME"), deviceID);

    if (pDebug)
        IDLog("Requesting to load default config with: %s\n", configDefaultFileName);

    pResult = IUReadConfig(configDefaultFileName, deviceID, errmsg) == 0 ? true : false;

    if (pResult)
        IDMessage(deviceID, "Default configuration loaded.");
    else
        IDMessage(deviceID, "Error loading default configuraiton. %s", errmsg);

    return pResult;
}

bool INDI::DefaultDriver::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{

    // ignore if not ours //
    if (strcmp (dev, deviceID))
        return false;

    ISwitchVectorProperty *svp = getSwitch(name);

    if (!svp)
        return false;

    if (!strcmp(svp->name, "DEBUG"))
    {
        IUUpdateSwitch(svp, states, names, n);
        ISwitch *sp = IUFindOnSwitch(svp);
        if (!sp)
            return false;

        if (!strcmp(sp->name, "ENABLE"))
            setDebug(true);
        else
            setDebug(false);
        return true;
    }

    if (!strcmp(svp->name, "SIMULATION"))
    {
        IUUpdateSwitch(svp, states, names, n);
        ISwitch *sp = IUFindOnSwitch(svp);
        if (!sp)
            return false;

        if (!strcmp(sp->name, "ENABLE"))
            setSimulation(true);
        else
            setSimulation(false);
        return true;
    }

    if (!strcmp(svp->name, "CONFIG_PROCESS"))
    {
        IUUpdateSwitch(svp, states, names, n);
        ISwitch *sp = IUFindOnSwitch(svp);
        IUResetSwitch(svp);
        bool pResult = false;
        if (!sp)
            return false;

        if (!strcmp(sp->name, "CONFIG_LOAD"))
            pResult = loadConfig();
        else if (!strcmp(sp->name, "CONFIG_SAVE"))
            pResult = saveConfig();
        else if (!strcmp(sp->name, "CONFIG_DEFAULT"))
            pResult = loadDefaultConfig();

        if (pResult)
            svp->s = IPS_OK;
        else
            svp->s = IPS_ALERT;

        IDSetSwitch(svp, NULL);
        return true;
    }

    return false;

}

void INDI::DefaultDriver::addDebugControl()
{
    /**************************************************************************/
    DebugSP = getSwitch("DEBUG");
    if (!DebugSP)
    {
        DebugSP = new ISwitchVectorProperty;
        IUFillSwitch(&DebugS[0], "ENABLE", "Enable", ISS_OFF);
        IUFillSwitch(&DebugS[1], "DISABLE", "Disable", ISS_ON);
        IUFillSwitchVector(DebugSP, DebugS, NARRAY(DebugS), deviceID, "DEBUG", "Debug", "Options", IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
        pSwitches.push_back(DebugSP);
        pOrder debo = {INDI_SWITCH, DebugSP};
        pAll.push_back(debo);
    }
    else
    {
        ISwitch *sp = IUFindSwitch(DebugSP, "ENABLE");
        if (sp)
            if (sp->s == ISS_ON)
                pDebug = true;
    }
    /**************************************************************************/

}

void INDI::DefaultDriver::addSimulationControl()
{
    /**************************************************************************/
    SimulationSP = getSwitch("SIMULATION");
    if (!SimulationSP)
    {
        SimulationSP = new ISwitchVectorProperty;
        IUFillSwitch(&SimulationS[0], "ENABLE", "Enable", ISS_OFF);
        IUFillSwitch(&SimulationS[1], "DISABLE", "Disable", ISS_ON);
        IUFillSwitchVector(SimulationSP, SimulationS, NARRAY(SimulationS), deviceID, "SIMULATION", "Simulation", "Options", IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
        pSwitches.push_back(SimulationSP);
        pOrder simo = {INDI_SWITCH, SimulationSP};
        pAll.push_back(simo);
    }
    else
    {
        ISwitch *sp = IUFindSwitch(SimulationSP, "ENABLE");
        if (sp)
            if (sp->s == ISS_ON)
                pSimulation = true;
    }
}

void INDI::DefaultDriver::addConfigurationControl()
{
    /**************************************************************************/
    ConfigProcessSP = getSwitch("CONFIG_PROCESS");
    if (!ConfigProcessSP)
    {
        ConfigProcessSP = new ISwitchVectorProperty;
        IUFillSwitch(&ConfigProcessS[0], "CONFIG_LOAD", "Load", ISS_OFF);
        IUFillSwitch(&ConfigProcessS[1], "CONFIG_SAVE", "Save", ISS_OFF);
        IUFillSwitch(&ConfigProcessS[2], "CONFIG_DEFAULT", "Default", ISS_OFF);
        IUFillSwitchVector(ConfigProcessSP, ConfigProcessS, NARRAY(ConfigProcessS), deviceID, "CONFIG_PROCESS", "Configuration", "Options", IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
        pSwitches.push_back(ConfigProcessSP);
        pOrder cpono = {INDI_SWITCH, ConfigProcessSP};
        pAll.push_back(cpono);
    }
    /**************************************************************************/

}

void INDI::DefaultDriver::addAuxControls()
{
   addDebugControl();
   addSimulationControl();
   addConfigurationControl();
}

void INDI::DefaultDriver::setDebug(bool enable)
{
    if (!DebugSP)
        return;

    if (pDebug == enable)
    {
        DebugSP->s = IPS_OK;
        IDSetSwitch(DebugSP, NULL);
        return;
    }

    IUResetSwitch(DebugSP);

    if (enable)
    {
        ISwitch *sp = IUFindSwitch(DebugSP, "ENABLE");
        if (sp)
        {
            sp->s = ISS_ON;
            IDMessage(deviceID, "Debug is enabled.");
        }
    }
    else
    {
        ISwitch *sp = IUFindSwitch(DebugSP, "DISABLE");
        if (sp)
        {
            sp->s = ISS_ON;
            IDMessage(deviceID, "Debug is disabled.");
        }
    }

    pDebug = enable;
    DebugSP->s = IPS_OK;
    IDSetSwitch(DebugSP, NULL);

}

void INDI::DefaultDriver::setSimulation(bool enable)
{
    if (!SimulationSP)
        return;

   if (pSimulation == enable)
   {
       SimulationSP->s = IPS_OK;
       IDSetSwitch(SimulationSP, NULL);
       return;
   }

   IUResetSwitch(SimulationSP);

   if (enable)
   {
       ISwitch *sp = IUFindSwitch(SimulationSP, "ENABLE");
       if (sp)
       {
           IDMessage(deviceID, "Simulation is enabled.");
           sp->s = ISS_ON;
       }
   }
   else
   {
       ISwitch *sp = IUFindSwitch(SimulationSP, "DISABLE");
       if (sp)
       {
           sp->s = ISS_ON;
           IDMessage(deviceID, "Simulation is disabled.");
       }
   }

   pSimulation = enable;
   SimulationSP->s = IPS_OK;
   IDSetSwitch(SimulationSP, NULL);

}

bool INDI::DefaultDriver::isDebug()
{
  return pDebug;
}

bool INDI::DefaultDriver::isSimulation()
{
 return pSimulation;
}

void INDI::DefaultDriver::ISGetProperties (const char *dev)
{
    std::vector<pOrder>::const_iterator orderi;

    for (orderi = pAll.begin(); orderi != pAll.end(); orderi++)
    {
        switch ( (*orderi).type)
        {
        case INDI_NUMBER:
             IDDefNumber(static_cast<INumberVectorProperty *>((*orderi).p) , NULL);
             break;
        case INDI_TEXT:
             IDDefText(static_cast<ITextVectorProperty *>((*orderi).p) , NULL);
             break;
        case INDI_SWITCH:
             IDDefSwitch(static_cast<ISwitchVectorProperty *>((*orderi).p) , NULL);
             break;
        case INDI_LIGHT:
             IDDefLight(static_cast<ILightVectorProperty *>((*orderi).p) , NULL);
             break;
        case INDI_BLOB:
             IDDefBLOB(static_cast<IBLOBVectorProperty *>((*orderi).p) , NULL);
             break;
        }
    }
}

void INDI::DefaultDriver::resetProperties()
{
    std::vector<INumberVectorProperty *>::const_iterator numi;
    std::vector<ISwitchVectorProperty *>::const_iterator switchi;
    std::vector<ITextVectorProperty *>::const_iterator texti;
    std::vector<ILightVectorProperty *>::const_iterator lighti;
    std::vector<IBLOBVectorProperty *>::const_iterator blobi;

    for ( numi = pNumbers.begin(); numi != pNumbers.end(); numi++)
    {
        (*numi)->s = IPS_IDLE;
        IDSetNumber( (*numi), NULL);
    }

   for ( switchi = pSwitches.begin(); switchi != pSwitches.end(); switchi++)
   {
       (*switchi)->s = IPS_IDLE;
       IDSetSwitch( (*switchi), NULL);
   }

   for ( texti = pTexts.begin(); texti != pTexts.end(); texti++)
   {
      (*texti)->s = IPS_IDLE;
      IDSetText( (*texti), NULL);
   }

   for ( lighti = pLights.begin(); lighti != pLights.end(); lighti++)
   {
       (*lighti)->s = IPS_IDLE;
       IDSetLight( (*lighti), NULL);
   }

   for ( blobi = pBlobs.begin(); blobi != pBlobs.end(); blobi++)
   {
       (*blobi)->s = IPS_IDLE;
       IDSetBLOB( (*blobi), NULL);
   }
}

void INDI::DefaultDriver::setConnected(bool status, IPState state, const char *msg)
{
    ISwitch *sp = NULL;
    ISwitchVectorProperty *svp = getSwitch("CONNECTION");
    if (!svp)
        return;

    IUResetSwitch(svp);

    // Connect
    if (status)
    {
        sp = IUFindSwitch(svp, "CONNECT");
        if (!sp)
            return;
        sp->s = ISS_ON;
    }
    // Disconnect
    else
    {
        sp = IUFindSwitch(svp, "DISCONNECT");
        if (!sp)
            return;
        sp->s = ISS_ON;
    }

    svp->s = state;

    IDSetSwitch(svp, msg, NULL);

}
